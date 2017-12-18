#include <stdlib.h>
#include <inttypes.h>
#include <string>
#include <set>
#include <tuple>
#include <memory>
#include <sstream>

#include "Identity.h"
#include "Destination.h"
#include "Datagram.h"
#include "Streaming.h"
#include "I2PService.h"
#include "api.h"

#include "i2pouichannel.h"
#include "service.h"

using namespace std;
using namespace ouichannel::i2p_ouichannel;

Channel::Channel(Service& service)
  : _ios(service.get_io_service()),
    _status_timer{_ios, std::chrono::seconds{3}},
    resolver_(_ios),
    socket_(_ios)
{
}

boost::asio::io_service& Channel::get_io_service()
{
    return _ios;
}

void Channel::connect( std::string target_id
                          , const std::string& shared_secret
                          , OnConnect connect_handler)
{
    _tunnel_port = rand() % 32768 + 32768;

    cout << "tunnel port: " << _tunnel_port << endl;

    i2p_oui_tunnel = std::make_unique<i2p::client::I2PClientTunnel>("i2p_oui_client", target_id, localhost, _tunnel_port, nullptr);
    _connect_handler = connect_handler;

    i2p_oui_tunnel->Start();
    //Wait till we find a route to the service and tunnel is ready then try to acutally connect and then call the handl
    i2p_oui_tunnel->AddReadyCallback(boost::bind(&Channel::handle_tunnel_ready, this, boost::asio::placeholders::error));
    _status_timer.async_wait(boost::bind(&Channel::handle_tunnel_ready, this, boost::asio::placeholders::error));

}

void Channel::listen(const std::string& shared_secret, int listen_port, OnConnect connect_handler, std::string private_key_str)
{
  _server_mode = true;
  _tunnel_port = listen_port;
  //we need to make a local destination first.
  //std::shared_ptr<i2p::client::ClientDestination>
  std::shared_ptr<i2p::client::ClientDestination> local_destination;
  

  if (private_key_str.length() > 0) {
    i2p::data::PrivateKeys service_keys;
    service_keys.FromBase64(private_key_str);
    local_destination = i2p::api::CreateLocalDestination(service_keys, true);
  }
  else {
    local_destination = i2p::api::CreateLocalDestination(true); 
  }
    
  i2p_oui_tunnel = std::make_unique<i2p::client::I2PServerTunnel>("i2p_oui_server", localhost, _tunnel_port, local_destination);
  _connect_handler = connect_handler;

  i2p_oui_tunnel->Start();
  //Wait till we find a route to the service and tunnel is ready then try to acutally connect and then call the handl
    i2p_oui_tunnel->GetLocalDestination()->GetIdentity();
    cout << "port: " << _tunnel_port << endl;
    cout << "i2p public id:"  << i2p_oui_tunnel->GetLocalDestination()->GetIdentity()->ToBase64() << endl;
    cout << "i2p private keys:"  << i2p_oui_tunnel->GetLocalDestination()->GetPrivateKeys().ToBase64() << endl;
    i2p_oui_tunnel->AddReadyCallback(boost::bind(&Channel::handle_tunnel_ready, this, boost::asio::placeholders::error));
    _status_timer.async_wait(boost::bind(&Channel::handle_tunnel_ready, this, boost::asio::placeholders::error));

}


void Channel::handle_tunnel_ready(const boost::system::error_code& err)
{
    if (!err)
    {
      std::cout << "Tunnel Ready? " << (i2p_oui_tunnel->GetLocalDestination()->IsReady() ? "True" : "False") << std::endl;
      std::cout.flush();

      if (i2p_oui_tunnel->GetLocalDestination()->IsReady() && (!_server_mode)) {

      // The tunnel is ready
      // Start an asynchronous resolve to translate the server and service names
      // into a list of endpoints.
      boost::asio::ip::tcp::resolver::query query(localhost, std::to_string(_tunnel_port));
      resolver_.async_resolve(query,
                              boost::bind(&Channel::handle_resolve, this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::iterator));
      } else {
        _status_timer.expires_at(_status_timer.expires_at() + std::chrono::seconds{3});
        _status_timer.async_wait(boost::bind(&Channel::handle_tunnel_ready, this, boost::asio::placeholders::error));
      }
    }
    else
    {
      std::cout << "Error: " << err.message() << "\n";
    }

}

void Channel::handle_resolve(const boost::system::error_code& err,
                             boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
  if (!err)
    {
      // Attempt a connection to the first endpoint in the list. Each endpoint
      // will be tried until we successfully establish a connection.
      boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
      socket_.async_connect(endpoint,
                            boost::bind(&Channel::handle_connect, this,
                                        boost::asio::placeholders::error));
    }
  else
    {
      std::cout << "Error: " << err.message() << "\n";
    }
}

void Channel::handle_connect(const boost::system::error_code& err)
{
  if (!err)
    {
      // The connection was successful. call the handler
      (_connect_handler)(err, this);
     
    }
  // else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
  //   {
  //     // The connection failed. Try the next endpoint in the list.
  //     socket_.close();
  //     boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
  //     socket_.async_connect(endpoint,
  //                           boost::bind(&Channel::handle_connect, this,
  //                                       boost::asio::placeholders::error, ++endpoint_iterator));
  //   }
    else
    {
      //connection failed call the handler with error
      std::cout << "Error: " << err.message() << "\n";
      (_connect_handler)(err, this);
    }
}

Channel::~Channel()
{
  //TODO
}
