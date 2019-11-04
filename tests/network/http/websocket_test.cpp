#include <boost/test/unit_test.hpp>

#include <boost/fiber/operations.hpp>

#include <fc/network/http/websocket.hpp>

#include <chrono>
#include <iostream>

#include "../../thread/worker_thread.hxx"

using namespace fc::test;

BOOST_GLOBAL_FIXTURE( worker_thread_config );

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE(websocket_test)
{ 
    fc::http::websocket_client client;
    fc::http::websocket_connection_ptr s_conn, c_conn;
    int port;
    {
        fc::http::websocket_server server;
        server.on_connection([&]( const fc::http::websocket_connection_ptr& c ){
                s_conn = c;
                c->on_message_handler([&](const std::string& s){
                    c->send_message("echo: " + s);
                });
            });

        server.listen( 0 );
        port = server.get_listening_port();

        server.start_accept();

        std::string echo;
        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        boost::this_fiber::sleep_for( std::chrono::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);
        c_conn->send_message( "again" );
        boost::this_fiber::sleep_for( std::chrono::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: again", echo);

        s_conn->close(0, "test");
        boost::this_fiber::sleep_for( std::chrono::milliseconds(100) );
        BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::exception);

        c_conn = client.connect( "ws://localhost:" + fc::to_string(port) );
        c_conn->on_message_handler([&](const std::string& s){
                    echo = s;
                });
        c_conn->send_message( "hello world" );
        boost::this_fiber::sleep_for( std::chrono::milliseconds(100) );
        BOOST_CHECK_EQUAL("echo: hello world", echo);
    }

    BOOST_CHECK_THROW(c_conn->send_message( "again" ), fc::assert_exception);
    BOOST_CHECK_THROW(client.connect( "ws://localhost:" + fc::to_string(port) ), fc::exception);
}

BOOST_AUTO_TEST_SUITE_END()
