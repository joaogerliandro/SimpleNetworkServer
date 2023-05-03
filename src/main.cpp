#include <iostream>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void new_session(tcp::socket socket)
{
    for (;;)
    {
        boost::asio::streambuf response_buffer;

        boost::asio::read_until(socket, response_buffer, "\n");

        std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

        response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

        std::cout << "[" << socket.remote_endpoint() << "-RESPONSE]: " << response_message << std::endl;

        boost::asio::write(socket, boost::asio::buffer("OK\n"));
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

            boost::asio::write(socket, boost::asio::buffer("Welcome new connection !\n"));

            std::thread(new_session, std::move(socket)).detach();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}