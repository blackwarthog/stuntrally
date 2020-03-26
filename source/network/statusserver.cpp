#include "pch.h"
#include "statusserver.hpp"
#include "xtime.hpp"

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <iomanip>
#include <sstream>


StatusServer::StatusServer(int port)
	: m_io()
	, m_acceptor(m_io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
	, m_mutex(), m_cond()
	, m_sendUpdates(true)
{
	handle_accept(
		boost::shared_ptr<Connection>(),
		boost::system::error_code() );
	m_statusSenderThread = boost::thread(boost::bind(&StatusServer::statusSenderThread, boost::ref(*this)));
	m_ioThread = boost::thread(boost::bind(&boost::asio::io_service::run, &m_io));
}

StatusServer::~StatusServer()
{
	// Shuts down running thread
	{
		boost::mutex::scoped_lock lock(m_mutex);
		m_sendUpdates = false;
	}
	m_cond.notify_all();
	m_statusSenderThread.join();
	m_io.stop();
	m_ioThread.join();
}

void StatusServer::handle_accept(
	boost::shared_ptr<Connection> connection,
	const boost::system::error_code& error )
{
	if (error) return;
	
	boost::mutex::scoped_lock lock(m_mutex);
	if (connection)
		m_connections.insert(connection);
	
	boost::shared_ptr<Connection> new_connection(new Connection(m_io));
	m_acceptor.async_accept(
		new_connection->m_socket,
		boost::bind(
			&StatusServer::handle_accept,
			this,
			new_connection,
			boost::asio::placeholders::error ));
}

void StatusServer::handle_write(
	boost::shared_ptr<Connection> connection,
	const boost::system::error_code& error,
	size_t /* transferred */ )
{
	if (error)
	{
		boost::mutex::scoped_lock lock(m_mutex);
		m_connections.erase(connection);
	}
}

void StatusServer::updateStatus(const Status &status)
{
	boost::mutex::scoped_lock lock(m_mutex);
	if ( !m_statusChanged
	  && ( fabs(status.odometer - m_status.odometer) >= 0.1
		|| fabs(status.longitude - m_status.longitude) >= 1e-7
		|| fabs(status.latitude - m_status.latitude) >= 1e-7 ))
	{
		m_statusChanged = true;
	}
	m_status = status;
}

void StatusServer::statusSenderThread()
{
	int idleCount = 0;
	std::string str;
	str.reserve(1024);
	
	std::set< boost::shared_ptr<Connection> >::const_iterator iter;
	
	boost::mutex::scoped_lock lock(m_mutex);
	while(m_sendUpdates)
	{
		// Broadcast status
		boost::xtime currentTime = now();
		if (++idleCount > 10 || m_statusChanged) {
			m_statusChanged = false;
			idleCount = 0;
			
			std::ostringstream ss;
			ss << "pc="
			   << (int)round(m_status.odometer) << ";"
			   << (long long)round(seconds(currentTime)*1000) << ";"
			   << std::setprecision(6)
			   << m_status.longitude << ";"
			   << m_status.latitude << "|" << std::endl;
			str = ss.str();
			
			for(iter = m_connections.begin(); iter != m_connections.end(); ++iter)
			{
				boost::asio::async_write(
					(*iter)->m_socket,
					boost::asio::buffer(str),
					boost::bind(
						&StatusServer::handle_write,
						this,
						*iter,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred ));
				
			}
		}
		m_cond.timed_wait(lock, currentTime + 0.1);
	}
}

