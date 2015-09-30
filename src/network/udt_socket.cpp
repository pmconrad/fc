#include <fc/network/udt_socket.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/unique_lock.hpp>
#include <udt.h>

#ifndef WIN32
# include <arpa/inet.h>
#endif

namespace fc {

   void check_udt_errors()
   {
      UDT::ERRORINFO& error_info = UDT::getlasterror();
      if( error_info.getErrorCode() )
      {
         std::string  error_message = error_info.getErrorMessage();
         error_info.clear();
         FC_CAPTURE_AND_THROW( udt_exception, (error_message) );
      }
   }
   
   class udt_epoll_service 
   {
      public:
         udt_epoll_service()
         :_epoll_thread("udt_epoll")
         {
            UDT::startup();
            check_udt_errors();
            _epoll_id = UDT::epoll_create();
            _epoll_loop = _epoll_thread.async( [=](){ poll_loop(); }, "udt_poll_loop" );
         }

         ~udt_epoll_service()
         {
            _epoll_loop.cancel("udt_epoll_service is destructing");
            _epoll_loop.wait();
            UDT::cleanup();
         }

         void poll_loop()
         {
            std::set<UDTSOCKET> read_ready;
            std::set<UDTSOCKET> write_ready;
            while( !_epoll_loop.canceled() )
            {
               UDT::epoll_wait( _epoll_id, 
                                &read_ready, 
                                &write_ready, 100000000 );

               { synchronized(_read_promises_mutex)
                  for( auto sock : read_ready )
                  {
                     auto itr = _read_promises.find( sock );
                     if( itr != _read_promises.end() )
                     {
                        itr->second->set_value();
                        _read_promises.erase(itr);
                     }
                  }
               } // synchronized read promise mutex

               { synchronized(_write_promises_mutex)
                  for( auto sock : write_ready )
                  {
                     auto itr = _write_promises.find( sock );
                     if( itr != _write_promises.end() )
                     {
                        itr->second->set_value();
                        _write_promises.erase(itr);
                     }
                  }
               } // synchronized write promise mutex
            } // while not canceled
         } // poll_loop


         void notify_read( int udt_socket_id, 
                           const promise<void>::ptr& p )
         {
            int events = UDT_EPOLL_IN | UDT_EPOLL_ERR;
            if( 0 != UDT::epoll_add_usock( _epoll_id, 
                                           udt_socket_id, 
                                           &events ) )
            {
               check_udt_errors();
            }
            { synchronized(_read_promises_mutex)

               _read_promises[udt_socket_id] = p;
            }
         }

         void notify_write( int udt_socket_id,
                            const promise<void>::ptr& p )
         {
            int events = UDT_EPOLL_OUT | UDT_EPOLL_ERR;
            if( 0 != UDT::epoll_add_usock( _epoll_id, 
                                  udt_socket_id, 
                                  &events ) )
            {
              check_udt_errors();
            }

            { synchronized(_write_promises_mutex)
               _write_promises[udt_socket_id] = p;
            }
         }
         void remove( int udt_socket_id )
         {
             { synchronized(_read_promises_mutex)
                auto read_itr = _read_promises.find( udt_socket_id );
                if( read_itr != _read_promises.end() )
                {
                   read_itr->second->set_exception( fc::copy_exception( fc::exception() ) );
                   _read_promises.erase(read_itr);
                }
             }
             { synchronized(_write_promises_mutex)
                auto write_itr = _write_promises.find( udt_socket_id );
                if( write_itr != _write_promises.end() )
                {
                   write_itr->second->set_exception( fc::copy_exception( fc::exception() ) );
                   _write_promises.erase(write_itr);
                }
             }
             UDT::epoll_remove_usock( _epoll_id, udt_socket_id );
         }

      private:
         fc::mutex                                    _read_promises_mutex;
         fc::mutex                                    _write_promises_mutex;
         std::unordered_map<int, promise<void>::ptr > _read_promises;
         std::unordered_map<int, promise<void>::ptr > _write_promises;

         fc::future<void> _epoll_loop;
         fc::thread _epoll_thread;
         int        _epoll_id;
   };


   udt_epoll_service& default_epool_service()
   {
      static udt_epoll_service* default_service = new udt_epoll_service();
      return *default_service;
   }



   udt_socket::udt_socket()
   :_udt_socket_id( UDT::INVALID_SOCK )
   {
   }

   udt_socket::~udt_socket()
   {
     try {
        close();
     } catch ( const fc::exception& e )
     {
        wlog( "${e}", ("e", e.to_detail_string() ) );
     }
   }

   void udt_socket::bind( const fc::ip::endpoint& local_endpoint )
   {
      bind( fc::ip::any_endpoint( local_endpoint.get_address(), local_endpoint.port() ) );
   }

   void udt_socket::bind( const fc::ip::any_endpoint& local_endpoint )
   { try {
      if( !is_open() ) 
         open();

      sockaddr_in6 local_addr;
      local_addr.sin6_family = AF_INET6;
      local_addr.sin6_port = htons(local_endpoint.port());
      fc::ip::raw_ip6 addr;
      if ( local_endpoint.get_address().get_type() == fc::ip::net_type::ipv4 )
      {
          addr = ip::raw_ip6( fc::ip::address_v6( local_endpoint.get_address().get_v4() ) );
      }
      else if ( local_endpoint.get_address().get_type() == fc::ip::net_type::ipv6 )
      {
          addr = ip::raw_ip6( local_endpoint.get_address().get_v6() );
      }
      else
      {
          FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
      }
      memcpy( &local_addr.sin6_addr, addr.begin(), sizeof(local_addr.sin6_addr) );

      if( UDT::ERROR == UDT::bind(_udt_socket_id, (sockaddr*)&local_addr, sizeof(local_addr)) )
         check_udt_errors();
   } FC_CAPTURE_AND_RETHROW() }

   void udt_socket::connect_to( const fc::ip::endpoint& remote_endpoint )
   {
      connect_to( fc::ip::any_endpoint( remote_endpoint.get_address(), remote_endpoint.port() ) );
   }

   void udt_socket::connect_to( const ip::any_endpoint& remote_endpoint )
   { try {
      if( !is_open() ) 
         open();

      sockaddr_in6 serv_addr;
      serv_addr.sin6_family = AF_INET6;
      serv_addr.sin6_port = htons(remote_endpoint.port());
      fc::ip::raw_ip6 addr;
      if ( remote_endpoint.get_address().get_type() == fc::ip::net_type::ipv4 )
      {
          addr = ip::raw_ip6(fc::ip::address_v6( remote_endpoint.get_address().get_v4() ) );
      }
      else if ( remote_endpoint.get_address().get_type() == fc::ip::net_type::ipv6 )
      {
          addr = ip::raw_ip6( remote_endpoint.get_address().get_v6() );
      }
      else
      {
          FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
      }
      memcpy( &serv_addr.sin6_addr, addr.begin(), sizeof(serv_addr.sin6_addr) );

      // UDT doesn't allow now blocking connects... 
      fc::thread connect_thread("connect_thread");
      connect_thread.async( [&](){
         if( UDT::ERROR == UDT::connect(_udt_socket_id, (sockaddr*)&serv_addr, sizeof(serv_addr)) )
            check_udt_errors();
      }, "udt_socket::connect_to").wait();

      bool block = false;
      UDT::setsockopt(_udt_socket_id, 0, UDT_SNDSYN, &block, sizeof(bool));
      UDT::setsockopt(_udt_socket_id, 0, UDT_RCVSYN, &block, sizeof(bool));
      check_udt_errors();

   } FC_CAPTURE_AND_RETHROW( (remote_endpoint) ) }

   ip::endpoint udt_socket::remote_endpoint() const
   {
      ip::any_endpoint ep = remote_endpoint_46();
      if ( ep.get_address().get_type() == ip::net_type::ipv4)
      {
        return fc::ip::endpoint( ep.get_address().get_v4(), ep.port() );
      }
      if ( ep.get_address().get_type() == ip::net_type::ipv6
           && ep.get_address().get_v6().is_mapped_v4() )
      {
        return fc::ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
      }
      FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
   }
   ip::any_endpoint udt_socket::remote_endpoint_46() const
   { try {
      sockaddr_in6 peer_addr;
      int peer_addr_size = sizeof(peer_addr);
      int error_code = UDT::getpeername( _udt_socket_id, (struct sockaddr*)&peer_addr, &peer_addr_size );
      if( error_code == UDT::ERROR )
          check_udt_errors();
     fc::ip::raw_ip6 raw;
     memcpy( raw.begin(), &peer_addr.sin6_addr, raw.size() );
     return ip::any_endpoint( ip::address_v6( raw ), htons(peer_addr.sin6_port) );
   } FC_CAPTURE_AND_RETHROW() }

   ip::endpoint udt_socket::local_endpoint() const
   {
      ip::any_endpoint ep = local_endpoint_46();
      if ( ep.get_address().get_type() == ip::net_type::ipv4)
      {
        return fc::ip::endpoint( ep.get_address().get_v4(), ep.port() );
      }
      if ( ep.get_address().get_type() == ip::net_type::ipv6
           && ep.get_address().get_v6().is_mapped_v4() )
      {
        return fc::ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
      }
      FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
   }
   ip::any_endpoint udt_socket::local_endpoint_46() const
   { try {
      sockaddr_in6 sock_addr;
      int addr_size = sizeof(sock_addr);
      int error_code = UDT::getsockname( _udt_socket_id, (struct sockaddr*)&sock_addr, &addr_size );
      if( error_code == UDT::ERROR )
          check_udt_errors();
     fc::ip::raw_ip6 raw;
     memcpy( raw.begin(), &sock_addr.sin6_addr, raw.size() );
     return ip::any_endpoint( ip::address_v6( raw ), htons(sock_addr.sin6_port) );
   } FC_CAPTURE_AND_RETHROW() }


   /// @{
   size_t   udt_socket::readsome( char* buffer, size_t max )
   { try {
      auto bytes_read = UDT::recv( _udt_socket_id, buffer, max, 0 );
      while( bytes_read == UDT::ERROR )
      {
         if( UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV )
         {
            UDT::getlasterror().clear();
            promise<void>::ptr p(new promise<void>("udt_socket::readsome"));
            default_epool_service().notify_read( _udt_socket_id, p );
            p->wait();
            bytes_read = UDT::recv( _udt_socket_id, buffer, max, 0 );
         }
         else
            check_udt_errors();
      }
      return bytes_read;
   } FC_CAPTURE_AND_RETHROW( (max) ) }

   size_t udt_socket::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset )
   {
     return readsome(buf.get() + offset, len);
   }

   bool     udt_socket::eof()const
   {
      // TODO... 
      return false;
   }
   /// @}
   
   /// ostream interface
   /// @{
   size_t   udt_socket::writesome( const char* buffer, size_t len )
   { try {
      auto bytes_sent = UDT::send(_udt_socket_id, buffer, len, 0);

      while( UDT::ERROR == bytes_sent )
      {
         if( UDT::getlasterror().getErrorCode() == CUDTException::EASYNCSND )
         {
            UDT::getlasterror().clear();
            promise<void>::ptr p(new promise<void>("udt_socket::writesome"));
            default_epool_service().notify_write( _udt_socket_id, p );
            p->wait();
            bytes_sent = UDT::send(_udt_socket_id, buffer, len, 0);
            continue;
         }
         else
            check_udt_errors();
      }
      return bytes_sent;
   } FC_CAPTURE_AND_RETHROW( (len) ) }

   size_t udt_socket::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset )
   {
     return writesome(buf.get() + offset, len);
   }

   void     udt_socket::flush(){}

   void     udt_socket::close()
   { try {
      if( is_open() )
      {
         default_epool_service().remove( _udt_socket_id );
         UDT::close( _udt_socket_id );
         check_udt_errors();
         _udt_socket_id = UDT::INVALID_SOCK;
      }
      else
      {
         wlog( "already closed" );
      }
   } FC_CAPTURE_AND_RETHROW() }
   /// @}
   
   void udt_socket::open()
   {
      _udt_socket_id = UDT::socket(AF_INET6, SOCK_STREAM, 0);
      if( _udt_socket_id == UDT::INVALID_SOCK )
         check_udt_errors();
   }

   bool udt_socket::is_open()const
   {
      return _udt_socket_id != UDT::INVALID_SOCK;
   }
     





  udt_server::udt_server()
  :_udt_socket_id( UDT::INVALID_SOCK )
  {
      _udt_socket_id = UDT::socket(AF_INET6, SOCK_STREAM, 0);
      if( _udt_socket_id == UDT::INVALID_SOCK )
         check_udt_errors();

      bool block = false;
      UDT::setsockopt(_udt_socket_id, 0, UDT_SNDSYN, &block, sizeof(bool));
      check_udt_errors();
      UDT::setsockopt(_udt_socket_id, 0, UDT_RCVSYN, &block, sizeof(bool));
      check_udt_errors();
  }

  udt_server::~udt_server()
  {
     try {
        close();
     } catch ( const fc::exception& e )
     {
        wlog( "${e}", ("e", e.to_detail_string() ) );
     }
  }

  void udt_server::close()
  { try {
     if( _udt_socket_id != UDT::INVALID_SOCK )
     {
        UDT::close( _udt_socket_id );
        check_udt_errors();
        default_epool_service().remove( _udt_socket_id );
        _udt_socket_id = UDT::INVALID_SOCK;
     }
  } FC_CAPTURE_AND_RETHROW() }

  void udt_server::accept( udt_socket& s )
  { try {
      FC_ASSERT( !s.is_open() );
      int namelen;
      sockaddr_in their_addr;


      while( s._udt_socket_id == UDT::INVALID_SOCK )
      {
         s._udt_socket_id = UDT::accept( _udt_socket_id, (sockaddr*)&their_addr, &namelen );
         if( UDT::getlasterror().getErrorCode() == CUDTException::EASYNCRCV )
         {
            UDT::getlasterror().clear();
            promise<void>::ptr p(new promise<void>("udt_server::accept"));
            default_epool_service().notify_read( _udt_socket_id, p );
            p->wait();
            s._udt_socket_id = UDT::accept( _udt_socket_id, (sockaddr*)&their_addr, &namelen );
         }
         else
            check_udt_errors();
      }
  } FC_CAPTURE_AND_RETHROW() }

  void udt_server::listen( const ip::endpoint& ep )
  {
      listen( fc::ip::any_endpoint( ep.get_address(), ep.port() ) );
  }
  void udt_server::listen( const ip::any_endpoint& ep )
  { try {
      sockaddr_in6 my_addr;
      my_addr.sin6_family = AF_INET6;
      my_addr.sin6_port = htons(ep.port());
      memset(&my_addr.sin6_addr, 0, sizeof(my_addr.sin6_addr));
      
      if( UDT::ERROR == UDT::bind(_udt_socket_id, (sockaddr*)&my_addr, sizeof(my_addr)) )
        check_udt_errors();

      UDT::listen(_udt_socket_id, 10);
      check_udt_errors();
  } FC_CAPTURE_AND_RETHROW( (ep) ) }

  fc::ip::endpoint udt_server::local_endpoint() const
  {
    fc::ip::any_endpoint ep = local_endpoint_46();
    if ( ep.get_address().get_type() == ip::net_type::ipv4)
    {
      return fc::ip::endpoint( ep.get_address().get_v4(), ep.port() );
    }
    if ( ep.get_address().get_type() == ip::net_type::ipv6
         && ep.get_address().get_v6().is_mapped_v4() )
    {
      return fc::ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
    }
    FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
  }
  fc::ip::any_endpoint udt_server::local_endpoint_46() const
  { try {
     sockaddr_in6 sock_addr;
     int addr_size = sizeof(sock_addr);
     int error_code = UDT::getsockname( _udt_socket_id, (struct sockaddr*)&sock_addr, &addr_size );
     if( error_code == UDT::ERROR )
         check_udt_errors();
     fc::ip::raw_ip6 raw;
     memcpy( raw.begin(), &sock_addr.sin6_addr, raw.size() );
     return ip::any_endpoint( ip::address_v6( raw ), htons(sock_addr.sin6_port) );
  } FC_CAPTURE_AND_RETHROW() }

} 
