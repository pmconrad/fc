#include <boost/test/unit_test.hpp>

#include <boost/fiber/operations.hpp>

#include <fc/network/http/websocket.hpp>

#include <chrono>
#include <iostream>

#include "../../thread/worker_thread.hxx"

using namespace fc::test;

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE(websocket_test)
{ 
    fc::http::websocket_client client;
    fc::http::websocket_connection_ptr s_conn, c_conn;
    fc::initialize_fibers();
    int port;
    bool server_init = false;
    bool client_init = false;
    boost::fibers::mutex mtx;
    boost::fibers::condition_variable cv;
    std::unique_lock<boost::fibers::mutex> lock(mtx);
    {
        fc::http::websocket_server server;
        server.on_connection([&s_conn,&server_init,&cv]( const fc::http::websocket_connection_ptr& c ){
                s_conn = c;
                c->on_message_handler([c](const std::string& s){
                    c->send_message("echo: " + s);
                });
                server_init = true;
                cv.notify_all();
            });

        server.listen( 0 );
        port = server.get_listening_port();

        server.start_accept();

        std::string echo;
        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        cv.wait( lock, [&server_init] () { return server_init; } );
        c_conn->on_message_handler([&echo,&client_init,&cv](const std::string& s){
           echo = s;
           client_init = true;
           cv.notify_all();
        });
        c_conn->send_message( "hello world" );
        cv.wait( lock, [&client_init] () { return client_init; } );
        BOOST_CHECK_EQUAL("echo: hello world", echo);

        client_init = false;
        c_conn->send_message( "again" );
        cv.wait( lock, [&client_init] () { return client_init; } );
        BOOST_CHECK_EQUAL("echo: again", echo);

        client_init = false;
        c_conn->closed.connect([&client_init,&cv] () {
           client_init = true;
           cv.notify_all();
        });
        s_conn->close(0, "test");
        cv.wait( lock, [&client_init] () { return client_init; } );
        BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::exception);

        server_init = false;
        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        cv.wait( lock, [&server_init] () { return server_init; } );
        c_conn->on_message_handler([&echo,&client_init,&cv](const std::string& s){
           echo = s;
           client_init = true;
           cv.notify_all();
        });
        client_init = false;
        c_conn->send_message( "hello world" );
        cv.wait( lock, [&client_init] () { return client_init; } );
        BOOST_CHECK_EQUAL("echo: hello world", echo);

        client_init = false;
        c_conn->closed.connect([&client_init,&cv] () {
           client_init = true;
           cv.notify_all();
        });
    }
    cv.wait( lock, [&client_init] () { return client_init; } );

    BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::assert_exception);
    BOOST_CHECK_THROW(client.connect( "ws://localhost:" + fc::to_string(port) ), fc::exception);
}

BOOST_AUTO_TEST_SUITE_END()
