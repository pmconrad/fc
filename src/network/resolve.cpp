#include <fc/asio.hpp>
#include <fc/network/ip.hpp>
#include <fc/log/logger.hpp>

namespace fc
{
  std::vector<fc::ip::endpoint> resolve( const fc::string& host, uint16_t port )
  {
    auto ep = fc::asio::tcp::resolve( host, std::to_string(uint64_t(port)) );
    std::vector<fc::ip::endpoint> eps;
    eps.reserve(ep.size());
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      if( itr->address().is_v4() )
      {
       eps.push_back( fc::ip::endpoint(itr->address().to_v4().to_ulong(), itr->port()) );
      }
      // TODO: add support for v6 
    }
    return eps;
  }
  std::vector<fc::ip::any_endpoint> resolve_46( const fc::string& host, uint16_t port )
  {
    auto ep = fc::asio::tcp::resolve( host, std::to_string(uint64_t(port)) );
    std::vector<fc::ip::any_endpoint> eps;
    eps.reserve(ep.size());
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      if( itr->address().is_v4() )
      {
       eps.push_back( fc::ip::any_endpoint(itr->address().to_v4().to_ulong(), itr->port()) );
      }
      else if( itr->address().is_v6() )
      {
       eps.push_back( fc::ip::any_endpoint(fc::ip::address_v6(itr->address().to_string()), itr->port() ) );
      }
    }
    return eps;
  }
}
