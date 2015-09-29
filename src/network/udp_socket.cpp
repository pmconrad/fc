#include <fc/network/udp_socket.hpp>
#include <fc/network/ip.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/asio.hpp>
#include <fc/exception/exception.hpp>

namespace fc {
  
  class udp_socket::impl : public fc::retainable {
    public:
      impl():_sock( fc::asio::default_io_service() ){}
      ~impl(){
      //  _sock.cancel();
      }

      boost::asio::ip::udp::socket _sock;
  };

  static boost::asio::ip::udp::endpoint to_asio_ep( const fc::ip::any_endpoint& e ) {
    fc::ip::address_v6 addr;
    switch (e.get_address().get_type()) {
        case fc::ip::net_type::ipv4:
           addr = fc::ip::address_v6(e.get_address().get_v4());
           break;
        case fc::ip::net_type::ipv6:
           addr = e.get_address().get_v6();
           break;
        default:
          FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported endpoint type");
    }
    return boost::asio::ip::udp::endpoint(boost::asio::ip::address_v6::from_string(fc::string(addr)), e.port() );
  }
  static fc::ip::any_endpoint to_fc_ep( const boost::asio::ip::udp::endpoint& e ) {
    if (e.address().is_v4()) {
      return fc::ip::any_endpoint( e.address().to_v4().to_ulong(), e.port() );
    }
    if (e.address().is_v6()) {
      return fc::ip::any_endpoint( fc::ip::address_v6(e.address().to_v6().to_string()), e.port() );
    }
    FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
  }

  udp_socket::udp_socket()
  :my( new impl() ) 
  {
  }

  udp_socket::udp_socket( const udp_socket& s )
  :my(s.my)
  {
  }

  udp_socket::~udp_socket() 
  {
    try 
    {
      my->_sock.close(); //close boost socket to make any pending reads run their completion handler
    }
    catch (...) //avoid destructor throw and likely this is just happening because socket wasn't open.
    {
    }
  }

  size_t udp_socket::send_to( const char* buffer, size_t length, const ip::endpoint& to ) 
  {
      return send_to( buffer, length, ip::any_endpoint( to.get_address(), to.port() ) );
  }

  size_t udp_socket::send_to( const char* buffer, size_t length, const ip::any_endpoint& to )
  {
    try 
    {
      return my->_sock.send_to( boost::asio::buffer(buffer, length), to_asio_ep(to) );
    } 
    catch( const boost::system::system_error& e ) 
    {
      if( e.code() != boost::asio::error::would_block )
        throw;
    }

    promise<size_t>::ptr completion_promise(new promise<size_t>("udp_socket::send_to"));
    my->_sock.async_send_to( boost::asio::buffer(buffer, length), to_asio_ep(to), 
                             asio::detail::read_write_handler(completion_promise) );

    return completion_promise->wait();
  }

  size_t udp_socket::send_to( const std::shared_ptr<const char>& buffer, size_t length, 
                              const fc::ip::endpoint& to )
  {
      return send_to( buffer, length, ip::any_endpoint( to.get_address(), to.port() ) );
  }

  size_t udp_socket::send_to( const std::shared_ptr<const char>& buffer, size_t length,
                              const fc::ip::any_endpoint& to )
  {
    try 
    {
      return my->_sock.send_to( boost::asio::buffer(buffer.get(), length), to_asio_ep(to) );
    } 
    catch( const boost::system::system_error& e ) 
    {
      if( e.code() != boost::asio::error::would_block )
        throw;
    }

    promise<size_t>::ptr completion_promise(new promise<size_t>("udp_socket::send_to"));
    my->_sock.async_send_to( boost::asio::buffer(buffer.get(), length), to_asio_ep(to), 
                             asio::detail::read_write_handler_with_buffer(completion_promise, buffer) );

    return completion_promise->wait();
  }

  void udp_socket::open() {
    my->_sock.open( boost::asio::ip::udp::v6() );
    my->_sock.non_blocking(true);
  }
  void udp_socket::set_receive_buffer_size( size_t s ) {
    my->_sock.set_option(boost::asio::socket_base::receive_buffer_size(s) );
  }
  void udp_socket::bind( const fc::ip::endpoint& e ) {
    bind( ip::any_endpoint( e.get_address(), e.port() ) );
  }
  void udp_socket::bind( const fc::ip::any_endpoint& e ) {
    my->_sock.bind( to_asio_ep(e) );
  }

  size_t udp_socket::receive_from( const std::shared_ptr<char>& receive_buffer, size_t receive_buffer_length, fc::ip::endpoint& from )
  {
    ip::any_endpoint ep;
    size_t res = receive_from( receive_buffer, receive_buffer_length, ep );
    if ( ep.get_address().get_type() == ip::net_type::ipv6
         && ep.get_address().get_v6().is_mapped_v4() )
    {
      from = ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
    }
    else if ( ep.get_address().get_type() == ip::net_type::ipv4 )
    {
      from = ip::endpoint( ep.get_address().get_v4(), ep.port() );
    }
    else
    {
      FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
    }
    return res;
  }

  size_t udp_socket::receive_from( const std::shared_ptr<char>& receive_buffer, size_t receive_buffer_length, fc::ip::any_endpoint& from )
  {
    try 
    {
      boost::asio::ip::udp::endpoint boost_from_endpoint;
      size_t bytes_read = my->_sock.receive_from( boost::asio::buffer(receive_buffer.get(), receive_buffer_length), 
                                                  boost_from_endpoint );
      from = to_fc_ep(boost_from_endpoint);
      return bytes_read;
    } 
    catch( const boost::system::system_error& e ) 
    {
      if( e.code() != boost::asio::error::would_block ) 
        throw;
    }

    boost::asio::ip::udp::endpoint boost_from_endpoint;
    promise<size_t>::ptr completion_promise(new promise<size_t>("udp_socket::receive_from"));
    my->_sock.async_receive_from( boost::asio::buffer(receive_buffer.get(), receive_buffer_length), 
                                  boost_from_endpoint,
                                  asio::detail::read_write_handler_with_buffer(completion_promise, receive_buffer) );
    size_t bytes_read = completion_promise->wait();
    from = to_fc_ep(boost_from_endpoint);
    return bytes_read;
  }

  size_t udp_socket::receive_from( char * receive_buffer, size_t receive_buffer_length, fc::ip::endpoint& from )
  {
    ip::any_endpoint ep;
    size_t res = receive_from( receive_buffer, receive_buffer_length, ep );
    if ( ep.get_address().get_type() == ip::net_type::ipv6
         && ep.get_address().get_v6().is_mapped_v4() )
    {
      from = ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
    }
    else if ( ep.get_address().get_type() == ip::net_type::ipv4 )
    {
      from = ip::endpoint( ep.get_address().get_v4(), ep.port() );
    }
    else
    {
      FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
    }
    return res;
  }

  size_t udp_socket::receive_from( char* receive_buffer, size_t receive_buffer_length, fc::ip::any_endpoint& from )
  {
    try 
    {
      boost::asio::ip::udp::endpoint boost_from_endpoint;
      size_t bytes_read = my->_sock.receive_from( boost::asio::buffer(receive_buffer, receive_buffer_length), 
                                                  boost_from_endpoint );
      from = to_fc_ep(boost_from_endpoint);
      return bytes_read;
    } 
    catch( const boost::system::system_error& e ) 
    {
      if( e.code() != boost::asio::error::would_block ) 
        throw;
    }

    boost::asio::ip::udp::endpoint boost_from_endpoint;
    promise<size_t>::ptr completion_promise(new promise<size_t>("udp_socket::receive_from"));
    my->_sock.async_receive_from( boost::asio::buffer(receive_buffer, receive_buffer_length), boost_from_endpoint,
                                  asio::detail::read_write_handler(completion_promise) );
    size_t bytes_read = completion_promise->wait();
    from = to_fc_ep(boost_from_endpoint);
    return bytes_read;
  }

  void   udp_socket::close() {
    //my->_sock.cancel(); 
    my->_sock.close();
  }

  fc::ip::endpoint udp_socket::local_endpoint()const {
    fc::ip::any_endpoint ep = local_endpoint_46();
    if ( ep.get_address().get_type() == fc::ip::net_type::ipv4)
    {
      return fc::ip::endpoint( ep.get_address().get_v4(), ep.port() );
    }
    if ( ep.get_address().get_type() == fc::ip::net_type::ipv6
         && ep.get_address().get_v6().is_mapped_v4() )
    {
      return fc::ip::endpoint( ep.get_address().get_v6().get_mapped_v4(), ep.port() );
    }
    FC_THROW_EXCEPTION(invalid_arg_exception, "unsupported address type");
  }

  fc::ip::any_endpoint udp_socket::local_endpoint_46()const {
    return to_fc_ep( my->_sock.local_endpoint() );
  }
  void udp_socket::connect( const fc::ip::endpoint& e ) {
   connect( ip::any_endpoint( e.get_address(), e.port() ) );
  }
  void udp_socket::connect( const fc::ip::any_endpoint& e ) {
    my->_sock.connect( to_asio_ep(e) );
  }

  void   udp_socket::set_multicast_enable_loopback( bool s )
  {
    my->_sock.set_option( boost::asio::ip::multicast::enable_loopback(s) );
  }
  void   udp_socket::set_reuse_address( bool s )
  {
    my->_sock.set_option( boost::asio::ip::udp::socket::reuse_address(s) );
  }
  void   udp_socket::join_multicast_group( const fc::ip::address& a )
  {
    join_multicast_group( fc::ip::any_address( a ) );
  }
  void   udp_socket::join_multicast_group( const fc::ip::any_address& a )
  {
    my->_sock.set_option( boost::asio::ip::multicast::join_group( boost::asio::ip::address::from_string(fc::string(a)) ) );
  }
}
