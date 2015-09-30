#include <fc/network/udt_socket.hpp>
#include <fc/network/ip.hpp>
#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>
#include <vector>

using namespace fc;

int main( int argc, char** argv )
{
   std::string hello = "hello world\n";

   try {
       udt_socket sock6;
       sock6.bind( fc::ip::any_endpoint::from_string( "[::1]:6666" ) );
       ilog( "." );
       sock6.connect_to( fc::ip::any_endpoint::from_string( "[::1]:7777" ) );
       ilog( "after connect to..." );

       std::cout << "local endpoint: " <<std::string( sock6.local_endpoint_46() ) <<"\n";
       std::cout << "remote endpoint: " <<std::string( sock6.remote_endpoint_46() ) <<"\n";

       for( uint32_t i = 0; i < 1000; ++i )
       {
          sock6.write( hello.c_str(), hello.size() );
       }
       ilog( "closing" );
       sock6.close();
   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }

   usleep( fc::seconds(1) );

   try {
       udt_socket sock;
       sock.bind( fc::ip::endpoint::from_string( "127.0.0.1:6666" ) );
       ilog( "." );
       sock.connect_to( fc::ip::endpoint::from_string( "127.0.0.1:7777" ) );
       ilog( "after connect to..." );

       std::cout << "local endpoint: " <<std::string( sock.local_endpoint() ) <<"\n";
       std::cout << "remote endpoint: " <<std::string( sock.remote_endpoint() ) <<"\n";

       for( uint32_t i = 0; i < 1000; ++i )
       {
          sock.write( hello.c_str(), hello.size() );
       }
       ilog( "closing" );
       sock.close();
   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }

    return 0;
}
