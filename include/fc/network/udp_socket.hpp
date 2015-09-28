#pragma once
#include <fc/network/ip.hpp>
#include <fc/utility.hpp>
#include <fc/shared_ptr.hpp>
#include <memory>

namespace fc {
  /**
   *  The udp_socket class has reference semantics, all copies will
   *  refer to the same underlying socket.
   */
  class udp_socket {
    public:
      udp_socket();
      udp_socket( const udp_socket& s );
      ~udp_socket();

      void   open();
      void   set_receive_buffer_size( size_t s );
      void   bind( const fc::ip::endpoint& );
      void   bind( const fc::ip::any_endpoint& );
      size_t receive_from( char* b, size_t l, fc::ip::endpoint& from );
      size_t receive_from( char* b, size_t l, fc::ip::any_endpoint& from );
      size_t receive_from( const std::shared_ptr<char>& b, size_t l, fc::ip::endpoint& from );
      size_t receive_from( const std::shared_ptr<char>& b, size_t l, fc::ip::any_endpoint& from );
      size_t send_to( const char* b, size_t l, const fc::ip::endpoint& to ); 
      size_t send_to( const char* b, size_t l, const fc::ip::any_endpoint& to );
      size_t send_to( const std::shared_ptr<const char>& b, size_t l, const fc::ip::endpoint& to );
      size_t send_to( const std::shared_ptr<const char>& b, size_t l, const fc::ip::any_endpoint& to );
      void   close();

      void   set_multicast_enable_loopback( bool );
      void   set_reuse_address( bool );
      void   join_multicast_group( const fc::ip::address& a );
      void   join_multicast_group( const fc::ip::any_address& a );

      void   connect( const fc::ip::endpoint& e );
      void   connect( const fc::ip::any_endpoint& e );
      fc::ip::endpoint local_endpoint()const;
      fc::ip::any_endpoint local_endpoint_46()const;

    private:
      class                impl;
      fc::shared_ptr<impl> my;
  };

}
