#pragma once

/**
 * @file
 * TODO: WriteMe.
 */

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio.hpp>
#include <set>

/**
 * @brief TODO: WriteMe
 *
 * This class is thread-safe.
 */
class StatusServer: public boost::noncopyable {
public:
	struct Status {
		double odometer;
		double longitude;
		double latitude;
		Status(): odometer(), longitude(), latitude() { }
	};
	
	class Connection: public boost::noncopyable
	{
	public:
		Connection(boost::asio::io_service& io_service):
			m_socket(io_service) { }
		boost::asio::ip::tcp::socket m_socket;
	};

	explicit StatusServer(int port = 11912);

	~StatusServer();

	/// Updates status it to send to clients
	void updateStatus(const Status &status);

	/// Thread that periodically broadcasts status, don't call directly
	void statusSenderThread();

	/// Handle new connection accept, don't call directly
	void handle_accept(
		boost::shared_ptr<Connection> connection,
		const boost::system::error_code& error );
  
	/// Handle ending of write to connection, don't call directly
	void handle_write(
		boost::shared_ptr<Connection> connection,
		const boost::system::error_code& error,
		size_t transferred );
	
private:
	boost::asio::io_service m_io;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set< boost::shared_ptr<Connection> > m_connections;
	boost::thread m_ioThread;
	
	mutable boost::mutex m_mutex;
	boost::condition m_cond;
	boost::thread m_statusSenderThread;
	Status m_status;
	bool m_statusChanged;
	bool m_sendUpdates;
};

