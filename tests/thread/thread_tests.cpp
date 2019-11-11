
#include <boost/test/unit_test.hpp>

#include <boost/thread/tss.hpp>

#include <fc/thread/async.hpp>
#include <fc/thread/fibers.hpp>

#include "worker_thread.hxx"

using namespace fc;
using namespace fc::test;

boost::thread_specific_ptr<bool> is_initialized( [] ( bool* v ) { v = nullptr; } );
void fc::test::init_rr_scheduler()
{
   static bool dummy;
   if( is_initialized.get() ) { return; }
   is_initialized.reset( &dummy );
   initialize_fibers();
}

BOOST_GLOBAL_FIXTURE( worker_thread_config );

BOOST_AUTO_TEST_SUITE(thread_tests)

BOOST_AUTO_TEST_CASE(executes_task)
{
    bool called = false;
    worker_thread thread;
    async( [&called] { called = true; }, thread.id() ).wait();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(returns_value_from_function)
{
    worker_thread thread;
    BOOST_CHECK_EQUAL(10, async( [] { return 10; }, thread.id() ).get());
}

BOOST_AUTO_TEST_CASE(executes_multiple_tasks)
{
    bool called1 = false;
    bool called2 = false;

    worker_thread thread;
    auto future1 = async([&called1]{called1 = true;}, thread.id() );
    auto future2 = async([&called2]{called2 = true;}, thread.id() );

    future2.wait();
    future1.wait();

    BOOST_CHECK(called1);
    BOOST_CHECK(called2);
}

BOOST_AUTO_TEST_CASE(calls_tasks_in_order)
{
    std::string result;

    worker_thread thread;
    auto future1 = async([&result]{result += "hello ";}, thread.id() );
    auto future2 = async([&result]{result += "world";}, thread.id() );

    future2.wait();
    future1.wait();

    BOOST_CHECK_EQUAL("hello world", result);
}

BOOST_AUTO_TEST_CASE(yields_execution)
{
    std::string result;

    worker_thread thread;
    auto future1 = async([&result]{boost::this_fiber::yield(); result += "world";}, thread.id() );
    auto future2 = async([&result]{result += "hello ";}, thread.id() );

    future2.wait();
    future1.wait();

    BOOST_CHECK_EQUAL("hello world", result);
}

BOOST_AUTO_TEST_CASE(reschedules_yielded_task)
{
    int reschedule_count = 0;

    worker_thread thread;
    auto future = async([&reschedule_count]
            {
                while (reschedule_count < 10)
                {
                    boost::this_fiber::yield(); 
                    reschedule_count++;
                }
            }, thread.id() );

    future.wait();
    BOOST_CHECK_EQUAL(10, reschedule_count);
}

BOOST_AUTO_TEST_SUITE_END()
