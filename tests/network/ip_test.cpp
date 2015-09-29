#include <boost/test/unit_test.hpp>

#include <fc/network/ip.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/thread.hpp>

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE( ip4_test )
{
    fc::ip::address any;
    BOOST_CHECK_EQUAL( 0, uint32_t(any) );
    BOOST_CHECK( !any.is_private_address() );
    BOOST_CHECK( !any.is_multicast_address() );
    BOOST_CHECK( !any.is_localhost() );

    fc::ip::address localhost( "127.0.0.1" );
    uint32_t local_ip = uint32_t(localhost);
    BOOST_CHECK_EQUAL( ((127 << 24) + 1), local_ip );
    BOOST_CHECK( localhost.is_localhost() );

    fc::ip::address other( local_ip );
    BOOST_CHECK( localhost == other );

    other = "10.1.2.3";
    BOOST_CHECK( localhost != other );
    BOOST_CHECK( other.is_private_address() );
    BOOST_CHECK( !other.is_public_address() );
    BOOST_CHECK( !other.is_localhost() );
    BOOST_CHECK_EQUAL( "10.1.2.3", fc::string(other) );


    fc::ip::endpoint listen;
    BOOST_CHECK_EQUAL( any, listen.get_address() );
    BOOST_CHECK_EQUAL( "0.0.0.0:0", fc::string(listen) );
    listen.set_port( 42 );
    BOOST_CHECK_EQUAL( 42, listen.port() );

    fc::ip::endpoint here( fc::ip::address( "127.0.0.1" ), 42 );
    fc::ip::endpoint there = fc::ip::endpoint::from_string( "127.0.0.1:42" );
    BOOST_CHECK( here == there );
    BOOST_CHECK( here != listen );
    BOOST_CHECK( listen < here );
    there.set_port( 43 );
    BOOST_CHECK( here != there );
    BOOST_CHECK( here < there );
    BOOST_CHECK_EQUAL( localhost, here.get_address() );
}

BOOST_AUTO_TEST_CASE( ip6_test )
{
    static const unsigned char EMPTY[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static unsigned char V4_MAP[16] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff,0,0,0,0};
    fc::ip::address_v6 any;
    BOOST_CHECK_EQUAL( 0, memcmp( fc::ip::raw_ip6(any).begin(), EMPTY, 16 ) );
    BOOST_CHECK_EQUAL( "::", fc::string(any) );
    BOOST_CHECK( !any.is_localhost() );
    BOOST_CHECK( !any.is_multicast_address() );
    BOOST_CHECK( !any.is_private_address() );
    BOOST_CHECK( !any.is_public_address() );
    BOOST_CHECK( any.is_mapped_v4() );

    fc::ip::address_v6 localhost( "::1" );
    BOOST_CHECK( localhost.is_localhost() );
    BOOST_CHECK( !localhost.is_multicast_address() );
    BOOST_CHECK( localhost.is_private_address() );
    BOOST_CHECK( !localhost.is_public_address() );
    BOOST_CHECK( !localhost.is_mapped_v4() );

    fc::ip::address_v6 localhost2 = localhost;
    localhost = "::ffff:127.0.0.1";
    BOOST_CHECK( localhost.is_localhost() );
    BOOST_CHECK( !localhost.is_multicast_address() );
    BOOST_CHECK( localhost.is_private_address() );
    BOOST_CHECK( !localhost.is_public_address() );
    BOOST_CHECK( localhost.is_mapped_v4() );
    BOOST_CHECK_EQUAL( (127 << 24) + 1, uint32_t(localhost.get_mapped_v4()) );

    fc::ip::address priv( "192.168.9.10" );
    fc::ip::address_v6 priv6( priv );
    BOOST_CHECK( !priv6.is_localhost() );
    BOOST_CHECK( !priv6.is_multicast_address() );
    BOOST_CHECK( priv6.is_private_address() );
    BOOST_CHECK( !priv6.is_public_address() );
    BOOST_CHECK( priv6.is_mapped_v4() );
    BOOST_CHECK_EQUAL( priv, priv6.get_mapped_v4() );
    BOOST_CHECK_EQUAL( "::ffff:192.168.9.10", fc::string(priv6) );
    V4_MAP[12] = 192; V4_MAP[13] = 168; V4_MAP[14] = 9; V4_MAP[15] = 10;
    BOOST_CHECK( 0 == memcmp( V4_MAP, fc::ip::raw_ip6(priv6).begin(), 16 ) );

    fc::ip::raw_ip6 raw;
    memcpy( raw.begin(), V4_MAP, raw.size() );
    fc::ip::address_v6 other(raw);
    BOOST_CHECK( priv6 == other );
    raw.begin()[15]++;
    BOOST_CHECK( priv6 != fc::ip::address_v6(raw) );


    fc::ip::endpoint_v6 listen;
    BOOST_CHECK( any == listen.get_address() );
    BOOST_CHECK_EQUAL( "[::]:0", fc::string(listen) );
    listen.set_port( 42 );
    BOOST_CHECK_EQUAL( 42, listen.port() );
    BOOST_CHECK_EQUAL( "[::]:42", fc::string(listen) );

    fc::ip::endpoint_v6 here( fc::ip::address_v6( "::1" ), 42 );
    fc::ip::endpoint_v6 there = fc::ip::endpoint_v6::from_string( "[::1]:42" );
    BOOST_CHECK( here == there );
    BOOST_CHECK( here != listen );
    BOOST_CHECK( listen < here );
    there.set_port( 43 );
    BOOST_CHECK( here != there );
    BOOST_CHECK( here < there );
    BOOST_CHECK( localhost2 == here.get_address() );
}

BOOST_AUTO_TEST_CASE( any_test )
{
    fc::ip::any_address local4( "127.0.0.1" );
    BOOST_CHECK( fc::ip::net_type::ipv4 == local4.get_type() );
    BOOST_CHECK( local4.is_localhost() );
    BOOST_CHECK( !local4.is_multicast_address() );
    BOOST_CHECK_EQUAL( "127.0.0.1", fc::string(local4) );

    fc::ip::any_address local6( "::1" );
    BOOST_CHECK( fc::ip::net_type::ipv6 == local6.get_type() );
    BOOST_CHECK( local6.is_localhost() );
    BOOST_CHECK( !local6.is_public_address() );
    BOOST_CHECK( !local6.is_multicast_address() );
    BOOST_CHECK( local4 != local6 );
    BOOST_CHECK_EQUAL( "::1", fc::string(local6) );

    fc::ip::address other4( "127.0.0.1" );
    fc::ip::any_address other( other4 );
    BOOST_CHECK( fc::ip::net_type::ipv4 == other.get_type() );
    BOOST_CHECK( local4 == other );

    other = fc::ip::any_address( fc::ip::address_v6( other4 ) );
    BOOST_CHECK( fc::ip::net_type::ipv6 == other.get_type() );
    BOOST_CHECK( local4 == other );

    fc::ip::address_v6 other6( "::1" );
    other = fc::ip::any_address( other6 );
    BOOST_CHECK( fc::ip::net_type::ipv6 == other.get_type() );
    BOOST_CHECK( local6 == other );

    BOOST_CHECK_EQUAL( other4, local4.get_v4() );
    BOOST_CHECK( other6 == local6.get_v6() );

    other = "::ffff:127.0.0.1";
    BOOST_CHECK( local4 == other );
    BOOST_CHECK( fc::ip::net_type::ipv6 == other.get_type() );

    other = "::ffff:172.16.17.18";
    BOOST_CHECK( !other.is_localhost() );
    BOOST_CHECK( other.is_private_address() );
    BOOST_CHECK( !other.is_public_address() );
    BOOST_CHECK( !other.is_multicast_address() );

    other = "2002:a9fe:1112::abcd";
    BOOST_CHECK( !other.is_localhost() );
    BOOST_CHECK( other.is_private_address() );
    BOOST_CHECK( !other.is_public_address() );
    BOOST_CHECK( !other.is_multicast_address() );

    other = "169.254.17.18";
    BOOST_CHECK( fc::ip::net_type::ipv4 == other.get_type() );
    BOOST_CHECK( !other.is_localhost() );
    BOOST_CHECK( other.is_private_address() );
    BOOST_CHECK( !other.is_public_address() );
    BOOST_CHECK( !other.is_multicast_address() );


    fc::ip::any_endpoint listen4;
    fc::ip::any_endpoint listen6( fc::ip::net_type::ipv6 );
    BOOST_CHECK_EQUAL( "0.0.0.0:0", fc::string(listen4) );
    BOOST_CHECK_EQUAL( "[::]:0", fc::string(listen6) );

    fc::ip::any_endpoint listen4b( other4, 42 );
    BOOST_CHECK_EQUAL( 42, listen4b.port() );

    fc::ip::any_endpoint listen6b( local6, 43 );
    BOOST_CHECK_EQUAL( 43, listen6b.port() );
    listen6b.set_port( 44 );
    BOOST_CHECK_EQUAL( 44, listen6b.port() );
    BOOST_CHECK_EQUAL( "[::1]:44", fc::string(listen6b) );

    listen4 = fc::ip::any_endpoint::from_string( "169.254.17.18:19" );
    BOOST_CHECK( fc::ip::any_endpoint( other, 19 ) == listen4 );
    BOOST_CHECK( fc::ip::any_endpoint( other, 20 ) != listen4 );
    BOOST_CHECK( fc::ip::any_endpoint( other, listen4b.port() ) != listen4b );
    BOOST_CHECK( listen4 < fc::ip::any_endpoint( other, 20 ) );
    BOOST_CHECK( fc::ip::any_endpoint( other, listen4b.port() ) < listen4b );
}

BOOST_AUTO_TEST_SUITE_END()
