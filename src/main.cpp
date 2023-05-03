#include <iostream>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

int main(int argc, char *argv[])
{
    try
    {
        boost::asio::io_service io_service;

        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 4444));
        std::cout << "[SERVER]: Awaiting connections ..." << std::endl;

        for(;;)
        {
            tcp::socket socket(io_service);

            acceptor.accept(socket);

            std::cout << "[SERVER]: Connection established with " << socket.remote_endpoint() << std::endl;

            boost::asio::write(socket, boost::asio::buffer("Welcome new connection !\n"));

            for(;;)
            {
                boost::asio::streambuf response_buffer;

                boost::asio::read_until(socket, response_buffer, "\n");

                std::string message = boost::asio::buffer_cast<const char*>(response_buffer.data());

                std::cout << "[" << socket.remote_endpoint() << "-RESPONSE]: " << message << std::endl;

                boost::asio::write(socket, boost::asio::buffer("OK\n"));
            }
        }
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}