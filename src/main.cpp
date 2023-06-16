#include <iostream>
#include <vector>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Client
{
    public:
        Client(tcp::socket& socket) : m_socket(socket) {}

        tcp::socket& m_socket;
};

std::vector<Client> client_list;

void new_session(tcp::socket socket)
{
    client_list.push_back(Client(socket));

    for (;;)
    {
        boost::asio::streambuf response_buffer;

        boost::asio::read_until(socket, response_buffer, "\n");

        std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

        response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

        std::cout << "[" << socket.remote_endpoint() << "-RESPONSE]: " << response_message << std::endl;

        boost::asio::ip::port_type sender_port = socket.remote_endpoint().port();
        boost::asio::ip::address sender_adress = socket.remote_endpoint().address();

        std::string sender_ip = sender_adress.to_string() + ":" + std::to_string(sender_port);

        for(Client client : client_list)
        {
            if(client.m_socket.remote_endpoint().address() == sender_adress &&
               client.m_socket.remote_endpoint().port() == sender_port)
            {
                sender_ip = "localhost";
            }

            std::string global_mensage = "[" + sender_ip + "-RESPONSE]: " + response_message;
            
            boost::asio::write(client.m_socket, boost::asio::buffer(global_mensage + "\n"));
        }
    }
}

int main(int argc, char *argv[])
{
    try
    {
        boost::asio::io_service io_service;

        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 4444));
        std::cout << "[SERVER]: Awaiting connections ..." << std::endl;

        for (;;)
        {
            tcp::socket socket(io_service);

            acceptor.accept(socket);

            std::cout << "[SERVER]: Connection established with " << socket.remote_endpoint() << std::endl;

            boost::asio::write(socket, boost::asio::buffer("[SERVER-RESPONSE]: Welcome " + socket.remote_endpoint().address().to_string() + ":" + std::to_string(socket.remote_endpoint().port()) + " !\n"));

            std::thread(new_session, std::move(socket)).detach();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}