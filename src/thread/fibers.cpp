/*
 * Copyright (c) 2019 The BitShares Blockchain, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <fc/exception/exception.hpp>
#include <fc/thread/fibers.hpp>

#include <boost/fiber/fss.hpp>
#include <boost/fiber/operations.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/tss.hpp>

#include <functional>
#include <map>

namespace fc {
   namespace detail {
      class dispatcher
      {
      public:
         dispatcher() {}

      private:
         void enlist( target_thread_scheduler_base* scheduler )
         {
            const auto tid = boost::this_thread::get_id();
            std::unique_lock<std::mutex> lock( threads_mutex );
            FC_ASSERT( threads.find( tid ) == threads.end(), "Trying to enlist an already registered thread?!" );
            threads[tid] = scheduler;
         }

         void delist()
         {
            const auto tid = boost::this_thread::get_id();
            std::unique_lock<std::mutex> lock( threads_mutex );
            FC_ASSERT( threads.find( tid ) != threads.end(), "Trying to delist an unlisted thread?!" );
            threads.erase( tid );
         }

         void migrate_context( boost::fibers::context* ctx, boost::thread::id dest )noexcept
         {
            std::unique_lock<std::mutex> lock( threads_mutex );
            if( threads.find( dest ) == threads.end() ) return; // fiber stays in this thread
            ctx->detach();
            threads[dest]->add_fiber( ctx );
         }

         friend class fc::target_thread_scheduler_base;

         std::mutex threads_mutex;
         std::map< boost::thread::id, target_thread_scheduler_base* > threads;
      };

      dispatcher _global_dispatcher;
   }

   target_thread_properties::target_thread_properties( boost::fibers::context *ctx )
      : boost::fibers::fiber_properties( ctx )
   {
   }

   bool target_thread_properties::has_target_thread()const
   {
      return target_thread != boost::thread::id();
   }

   boost::thread::id target_thread_properties::get_target_thread()const
   {
      return target_thread;
   }

   void target_thread_properties::set_target_thread( boost::thread::id id )
   {
      target_thread = id;
   }

   target_thread_scheduler_base::target_thread_scheduler_base()
   {
      detail::_global_dispatcher.enlist( this );
   }

   target_thread_scheduler_base::~target_thread_scheduler_base()
   {
      detail::_global_dispatcher.delist();
   }

   void target_thread_scheduler_base::awakened( boost::fibers::context* ctx,
                                                target_thread_properties& props ) noexcept
   {
      const boost::thread::id target = props.get_target_thread();
      if( target == boost::thread::id() || target == boost::this_thread::get_id() )
         get_delegate().awakened( ctx );
      else
         detail::_global_dispatcher.migrate_context( ctx, target );
   }

   boost::fibers::context* target_thread_scheduler_base::pick_next() noexcept
   {
      requeue();
      boost::fibers::context* ctx = get_delegate().pick_next();
      if( !ctx ) return ctx;
      target_thread_properties* props = dynamic_cast<target_thread_properties*>( get_properties( ctx ) );
      if( !props ) return ctx;
      boost::thread::id target = props->get_target_thread();
      while( ctx && target != boost::thread::id() && target != boost::this_thread::get_id() )
      {
         detail::_global_dispatcher.migrate_context( ctx, target );
         ctx = get_delegate().pick_next();
         if( !ctx ) break;
         target_thread_properties* props = dynamic_cast<target_thread_properties*>( get_properties( ctx ) );
         if( !props ) break;
         target = props->get_target_thread();
      }
      return ctx;
   }

   bool target_thread_scheduler_base::has_ready_fibers() const noexcept
   {
      return get_delegate().has_ready_fibers() || !ready_queue.empty();
   }

   void target_thread_scheduler_base::suspend_until( const std::chrono::steady_clock::time_point& then ) noexcept
   {
      get_delegate().suspend_until( then );
   }

   void target_thread_scheduler_base::notify() noexcept
   {
      get_delegate().notify();
   }
   
   void target_thread_scheduler_base::requeue()noexcept
   {
      std::vector<boost::fibers::context*> tmp;
      ready_queue.consume_all( [&tmp] ( boost::fibers::context* ctx ) { tmp.emplace_back( ctx ); } );
      for( const auto ctx : tmp )
      {
         boost::fibers::context::active()->attach( ctx );
         awakened( ctx, *dynamic_cast<target_thread_properties*>( get_properties( ctx ) ) );
      }
   }
   
   void target_thread_scheduler_base::add_fiber( boost::fibers::context* ctx )noexcept
   {
      ready_queue.push( ctx );
      notify();
   }

   static boost::thread_specific_ptr<std::string> thread_name;
   static boost::thread_specific_ptr<std::string> thread_id;
   const std::string& get_thread_name()
   {
      if( thread_name.get() != nullptr ) return *thread_name.get();
      if( thread_id.get() == nullptr )
      {
         std::stringstream tid;
         tid << "thread #" << boost::this_thread::get_id();
         thread_id.reset( new std::string( tid.str() ) );
      }
      return *thread_id.get();
   }
   void set_thread_name( const std::string& name )
   {
      FC_ASSERT( thread_name.get() == nullptr, "Thread name already set!" );
      thread_name.reset( new std::string( name ) );
   }

   static boost::fibers::fiber_specific_ptr<std::string> fiber_name;
   static boost::fibers::fiber_specific_ptr<std::string> fiber_id;
   const std::string& get_fiber_name()
   {
      if( fiber_name.get() != nullptr ) return *fiber_name.get();
      if( fiber_id.get() == nullptr )
      {
         std::stringstream tid;
         tid << "fiber #" << boost::this_fiber::get_id();
         fiber_id.reset( new std::string( tid.str() ) );
      }
      return *fiber_id.get();
   }
   void set_fiber_name( const std::string& name )
   {
      FC_ASSERT( fiber_name.get() == nullptr, "Fiber name already set!" );
      fiber_name.reset( new std::string( name ) );
   }
} // fc
