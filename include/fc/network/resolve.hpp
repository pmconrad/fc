#pragma once
#include <fc/vector.hpp>
#include <fc/network/ip.hpp>

namespace fc
{
  std::vector<fc::ip::endpoint> resolve( const std::string& host, uint16_t port );
  std::vector<fc::ip::any_endpoint> resolve_46( const std::string& host, uint16_t port );
}
