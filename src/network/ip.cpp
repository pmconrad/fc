#include <fc/network/ip.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

namespace fc { namespace ip {

  address::address( uint32_t ip )
  :_ip(ip){}

  address::address( const fc::string& s ) 
  {
    try
    {
      _ip = boost::asio::ip::address_v4::from_string(s.c_str()).to_ulong();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }

  bool operator==( const address& a, const address& b ) {
    return uint32_t(a) == uint32_t(b);
  }
  bool operator!=( const address& a, const address& b ) {
    return uint32_t(a) != uint32_t(b);
  }

  address& address::operator=( const fc::string& s ) 
  {
    try
    {
      _ip = boost::asio::ip::address_v4::from_string(s.c_str()).to_ulong();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
    return *this;
  }

  address::operator fc::string()const 
  {
    try
    {
      return boost::asio::ip::address_v4(_ip).to_string().c_str();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address to string")
  }
  address::operator uint32_t()const {
    return _ip;
  }


  endpoint::endpoint()
  :_port(0){  }
  endpoint::endpoint(const address& a, uint16_t p)
  :_port(p),_ip(a){}

  bool operator==( const endpoint& a, const endpoint& b ) {
    return a._port == b._port  && a._ip == b._ip;
  }
  bool operator!=( const endpoint& a, const endpoint& b ) {
    return a._port != b._port || a._ip != b._ip;
  }

  bool operator< ( const endpoint& a, const endpoint& b )
  {
     return  uint32_t(a.get_address()) < uint32_t(b.get_address()) ||
             (uint32_t(a.get_address()) == uint32_t(b.get_address()) &&
              uint32_t(a.port()) < uint32_t(b.port()));
  }

  uint16_t       endpoint::port()const    { return _port; }
  const address& endpoint::get_address()const { return _ip;   }

  endpoint endpoint::from_string( const string& endpoint_string )
  {
    try
    {
      endpoint ep;
      auto pos = endpoint_string.find(':');
      ep._ip   = boost::asio::ip::address_v4::from_string(endpoint_string.substr( 0, pos ) ).to_ulong();
      ep._port = boost::lexical_cast<uint16_t>( endpoint_string.substr( pos+1, endpoint_string.size() ) );
      return ep;
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string to IP endpoint")
  }

  endpoint::operator string()const 
  {
    try
    {
      return string(_ip) + ':' + fc::string(boost::lexical_cast<std::string>(_port).c_str());
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
  }

  /**
   *  @return true if the ip is in the following ranges:
   *
   *  10.0.0.0    to 10.255.255.255
   *  172.16.0.0  to 172.31.255.255
   *  192.168.0.0 to 192.168.255.255
   *  169.254.0.0 to 169.254.255.255
   *
   */
  bool address::is_private_address()const
  {
    static address min10_ip("10.0.0.0");
    static address max10_ip("10.255.255.255");
    static address min172_ip("172.16.0.0");
    static address max172_ip("172.31.255.255");
    static address min192_ip("192.168.0.0");
    static address max192_ip("192.168.255.255");
    static address min169_ip("169.254.0.0");
    static address max169_ip("169.254.255.255");
    if( _ip >= min10_ip._ip && _ip <= max10_ip._ip ) return true;
    if( _ip >= min172_ip._ip && _ip <= max172_ip._ip ) return true;
    if( _ip >= min192_ip._ip && _ip <= max192_ip._ip ) return true;
    if( _ip >= min169_ip._ip && _ip <= max169_ip._ip ) return true;
    return false;
  }

  /**
   *  224.0.0.0 to 239.255.255.255
   */
  bool address::is_multicast_address()const
  {
    static address min_ip("224.0.0.0");
    static address max_ip("239.255.255.255");
    return  _ip >= min_ip._ip  && _ip <= max_ip._ip;
  }

  /** !private & !multicast */
  bool address::is_public_address()const
  {
    return !( is_private_address() || is_multicast_address() );
  }

  bool address::is_localhost()const
  {
    return (_ip >> 24) == 127;
  }


  static const unsigned char LOCALHOST[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
  static const unsigned char V4_PREFIX[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};

  address_v6::address_v6() {
      memset( &_ip, 0, sizeof( _ip ) );
  }

  address_v6::address_v6( const raw_ip6& ip )
  :_ip(ip){}

  address_v6::address_v6( const address& ip4 )
  {
      memcpy( _ip.begin(), V4_PREFIX, sizeof(V4_PREFIX) );
      uint32_t v4 = uint32_t(ip4);
      _ip.begin()[12] = v4 >> 24;
      _ip.begin()[13] = (v4 >> 16) & 0xff;
      _ip.begin()[14] = (v4 >> 8) & 0xff;
      _ip.begin()[15] = v4 & 0xff;
  }

  address_v6::address_v6( const fc::string& s )
  {
    try
    {
      memcpy( _ip.begin(), boost::asio::ip::address_v6::from_string(s.c_str()).to_bytes().data(), _ip.size() );
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }

  bool operator==( const address_v6& a, const address_v6& b ) {
    return memcmp( raw_ip6(a).begin(), raw_ip6(b).begin(), raw_ip6(a).size() ) == 0;
  }
  bool operator!=( const address_v6& a, const address_v6& b ) {
    return memcmp( raw_ip6(a).begin(), raw_ip6(b).begin(), raw_ip6(a).size() ) != 0;
  }

  address_v6& address_v6::operator=( const fc::string& s )
  {
    try
    {
      memcpy( _ip.begin(), boost::asio::ip::address_v6::from_string(s.c_str()).to_bytes().data(), _ip.size() );
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
    return *this;
  }

  address_v6::operator fc::string()const
  {
    try
    {
        boost::asio::ip::address_v6::bytes_type bytes;
        memcpy( bytes.data(), _ip.begin(), bytes.size() );
        boost::asio::ip::address_v6 ba( bytes );
        return ba.to_string();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address to string")
  }
  address_v6::operator raw_ip6()const {
    return _ip;
  }


  endpoint_v6::endpoint_v6()
  :_port(0){ }
  endpoint_v6::endpoint_v6(const address_v6& a, uint16_t p)
  :_port(p),_ip(a){}

  bool operator==( const endpoint_v6& a, const endpoint_v6& b ) {
    return a._port == b._port  && a._ip == b._ip;
  }
  bool operator!=( const endpoint_v6& a, const endpoint_v6& b ) {
    return a._port != b._port || a._ip != b._ip;
  }

  bool operator< ( const endpoint_v6& a, const endpoint_v6& b )
  {
     int d = memcmp( raw_ip6(a._ip).begin(), raw_ip6(b._ip).begin(), raw_ip6(a._ip).size() );
     return d < 0 || ( d == 0 && uint32_t(a.port()) < uint32_t(b.port()) );
  }

  uint16_t          endpoint_v6::port()const    { return _port; }
  const address_v6& endpoint_v6::get_address()const { return _ip;   }

  endpoint_v6 endpoint_v6::from_string( const string& endpoint_string )
  {
    try
    {
      if ( endpoint_string.at(0) == '[' )
      {
        auto pos = endpoint_string.find("]:");
        if ( pos != string::npos )
        {
          endpoint_v6 ep;
          ep._ip = endpoint_string.substr( 1, pos - 1 );
          ep._port = boost::lexical_cast<uint16_t>( endpoint_string.substr( pos+2, endpoint_string.size() ) );
          return ep;
        }
      }
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string to IP endpoint")
    FC_THROW_EXCEPTION(parse_error_exception, "error converting string to IP endpoint");
  }

  endpoint_v6::operator string()const
  {
    try
    {
      return '[' + string(_ip) + "]:" + fc::string(boost::lexical_cast<std::string>(_port).c_str());
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
  }

  static address from_bytes( const unsigned char *bytes )
  {
      address v4(   (bytes[0] << 24)
                  | (bytes[1] << 16)
                  | (bytes[2] << 8)
                  |  bytes[3] );
      return v4;
  }

  bool address_v6::is_private_address()const
  {
    if ( *_ip.begin() == 0x20 && _ip.begin()[1] == 2 ) {
        // 6to4
        return from_bytes( _ip.begin() + 2 ).is_private_address();
    }
    return is_localhost()
           || (is_mapped_v4() && get_mapped_v4().is_private_address())
           || (*_ip.begin() & 0xfe) == 0xfc
           || (*_ip.begin() == 0xfe
               && (_ip.begin()[1] & 0x80) == 0x80);
  }

  bool address_v6::is_multicast_address()const
  {
    return *_ip.begin() == 0xff;
  }

  bool address_v6::is_public_address()const
  {
    return !is_private_address() && (*_ip.begin() & 0xe0) == 0x20;
  }

  bool address_v6::is_localhost()const
  {
      return memcmp( _ip.begin(), LOCALHOST, _ip.size() ) == 0
             || (is_mapped_v4() && (uint32_t(get_mapped_v4()) >> 24) == 127);
  }

  bool address_v6::is_mapped_v4()const
  {
      return memcmp( _ip.begin(), V4_PREFIX, sizeof(V4_PREFIX) ) == 0;
  }

  address address_v6::get_mapped_v4()const
  {
      FC_ASSERT( is_mapped_v4() );
      return from_bytes( _ip.begin() + 12 );
  }


  any_address::any_address( net_type t ) : type(t) {}
  any_address::any_address( const address& ip4 ) : type(ipv4), v4(ip4) {}
  any_address::any_address( const address_v6& ip6 ) : type(ipv6), v6(ip6) {}
  any_address::any_address( const any_address& addr )
    : type(addr.get_type()), v4(addr.get_v4()), v6(addr.get_v6()) {}
  any_address::any_address( const fc::string& s )
  {
    if ( s.find(':') == string::npos )
    {
        type = ipv4;
        v4 = s;
    }
    else
    {
        type = ipv6;
        v6 = s;
    }
  }

  bool operator==( const any_address& a, const any_address& b ) {
      if ( a.get_type() == ipv4 && b.get_type() == ipv4 )
      {
          return a.get_v4() == b.get_v4();
      }
      if ( a.get_type() == ipv6 && b.get_type() == ipv6 )
      {
          return a.get_v6() == b.get_v6();
      }
      return a.get_type() == ipv4 ? b.get_v6().is_mapped_v4()
                                    && b.get_v6().get_mapped_v4() == a.get_v4()
                                  : a.get_v6().is_mapped_v4()
                                    && a.get_v6().get_mapped_v4() == b.get_v4();
  }
  bool operator!=( const any_address& a, const any_address& b ) {
    return !(a == b);
  }

  any_address& any_address::operator=( const fc::string& s )
  {
    if ( s.find(':') == string::npos )
    {
        type = ipv4;
        v4 = s;
    }
    else
    {
        type = ipv6;
        v6 = s;
    }
    return *this;
  }

  any_address::operator fc::string()const
  {
      return type == ipv4 ? fc::string(v4) : fc::string(v6);
  }

  net_type any_address::get_type() const { return type; }
  const address& any_address::get_v4() const { return v4; }
  const address_v6& any_address::get_v6() const { return v6; }


  any_endpoint::any_endpoint( net_type t )
  :_port(0),_ip(t){}
  any_endpoint::any_endpoint(const address& a, uint16_t p)
  :_port(p),_ip(a){}
  any_endpoint::any_endpoint(const address_v6& a, uint16_t p)
  :_port(p),_ip(a){}
  any_endpoint::any_endpoint(const any_address& a, uint16_t p)
  :_port(p),_ip(a){}

  bool operator==( const any_endpoint& a, const any_endpoint& b ) {
    return a._port == b._port && a._ip == b._ip;
  }
  bool operator!=( const any_endpoint& a, const any_endpoint& b ) {
    return a._port != b._port || a._ip != b._ip;
  }

  bool operator< ( const any_endpoint& a, const any_endpoint& b )
  {
     //FIXME
  }

  uint16_t           any_endpoint::port()const        { return _port; }
  const any_address& any_endpoint::get_address()const { return _ip;   }

  any_endpoint any_endpoint::from_string( const string& endpoint_string )
  {
    try
    {
      if ( endpoint_string.at(0) == '[' )
      {
          endpoint_v6 ep6 = endpoint_v6::from_string( endpoint_string );
          any_endpoint ep( ep6.get_address(), ep6.port() );
          return ep;
      }
      endpoint ep4 = endpoint::from_string( endpoint_string );
      any_endpoint ep( ep4.get_address(), ep4.port() );
      return ep;
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string to IP endpoint")
    FC_THROW_EXCEPTION(parse_error_exception, "error converting string to IP endpoint");
  }

  any_endpoint::operator string()const
  {
    try
    {
      return (_ip.get_type() == ipv4 ? string(_ip) : '[' + string(_ip) + ']')
             + ':' + fc::string(boost::lexical_cast<std::string>(_port).c_str());
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
  }

  bool any_address::is_private_address()const
  {
      return type == ipv4 ? v4.is_private_address() : v6.is_private_address();
  }

  bool any_address::is_multicast_address()const
  {
      return type == ipv4 ? v4.is_multicast_address() : v6.is_multicast_address();
  }

  bool any_address::is_public_address()const
  {
      return type == ipv4 ? v4.is_public_address() : v6.is_public_address();
  }

  bool any_address::is_localhost()const
  {
      return type == ipv4 ? v4.is_localhost() : v6.is_localhost();
  }
}  // namespace ip

  void to_variant( const ip::endpoint& var,  variant& vo )
  {
      vo = fc::string(var);
  }
  void from_variant( const variant& var,  ip::endpoint& vo )
  {
     vo = ip::endpoint::from_string(var.as_string());
  }

  void to_variant( const ip::address& var,  variant& vo )
  {
    vo = fc::string(var);
  }
  void from_variant( const variant& var,  ip::address& vo )
  {
    vo = ip::address(var.as_string());
  }

  void to_variant( const ip::endpoint_v6& var, variant& vo )
  {
      vo = fc::string(var);
  }
  void from_variant( const variant& var, ip::endpoint_v6& vo )
  {
     vo = ip::endpoint_v6::from_string(var.as_string());
  }

  void to_variant( const ip::address_v6& var, variant& vo )
  {
    vo = fc::string(var);
  }
  void from_variant( const variant& var, ip::address_v6& vo )
  {
    vo = ip::address_v6(var.as_string());
  }

  void to_variant( const ip::any_endpoint& var, variant& vo )
  {
      vo = fc::string(var);
  }
  void from_variant( const variant& var, ip::any_endpoint& vo )
  {
     vo = ip::any_endpoint::from_string(var.as_string());
  }

  void to_variant( const ip::any_address& var, variant& vo )
  {
    vo = fc::string(var);
  }
  void from_variant( const variant& var, ip::any_address& vo )
  {
    vo = ip::any_address(var.as_string());
  }
} 
