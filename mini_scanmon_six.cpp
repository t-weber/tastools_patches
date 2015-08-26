/*
 * clang -o mini_scanmon_six mini_scanmon_six.cpp ../../tlibs/net/tcp.cpp ../../tlibs/helper/log.cpp -std=c++11 -lstdc++ -lm -lboost_system -lboost_iostreams -lpthread
 */

#include <iostream>
#include <iomanip>
#include <fstream>

#include "../../tlibs/net/tcp.h"
#include "../../tlibs/helper/log.h"
#include "../../tlibs/string/string.h"

using namespace tl;
typedef float t_real;



// --------------------------------------------------------------------------------------
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

namespace ios = boost::iostreams;


FILE *pipeProg = nullptr;
boost::iostreams::file_descriptor_sink *pfds = nullptr;
boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink> *psbuf = nullptr;
std::ostream *postrProg = nullptr;


void close_progress()
{
	if(pipeProg)
	{
		pclose(pipeProg);
		pipeProg = nullptr;
	}

	if(postrProg) { delete postrProg; postrProg = nullptr; }
	if(psbuf) { delete psbuf; psbuf = nullptr; }
	if(pfds) { delete pfds; pfds = nullptr; }
}

bool open_progress()
{
	close_progress();

	pipeProg = (FILE*)popen("dialog --title \"Scan Progress\" --gauge \"\" 10 50 0", "w");
	if(!pipeProg) return false;
	pfds = new ios::file_descriptor_sink(fileno(pipeProg), ios::close_handle);
	psbuf = new ios::stream_buffer<ios::file_descriptor_sink>(*pfds);
	postrProg = new std::ostream(psbuf);

	return true;
}

void set_progress(int iPerc, const std::string& strTxt)
{
	if(!postrProg) return;
	(*postrProg) << "XXX\n" << iPerc << "\n"
		<< strTxt << "\nXXX"
		<< std::endl;
}
// --------------------------------------------------------------------------------------



t_real dCtr = 0.;
t_real dMon = 0.;
t_real dSel = 0.;


void refresh()
{
	t_real dProgress = 0.;
	t_real dExpCtr = 0.;

	if(!float_equal(dSel, t_real(0.)))
	{
		dProgress = dMon / dSel;
		dExpCtr = dCtr / dProgress;
	}

	std::ostringstream ostr;
	ostr.precision(2);

	/*ostr << "\r"
		<< "Counts: " << std::fixed << dCtr
		<< ", Expected: " << std::fixed << dExpCtr
		<< ", Scan progress: " << std::fixed << dProgress*100. << " % ("
		<< std::fixed << dMon << " of " << dSel << " monitor counts)."
		<< "        ";

	std::cout << ostr.str();
	std::cout.flush();*/

	ostr << "Counts:   " << std::fixed << dCtr << "\n";
	ostr << "Expected: " << std::fixed << dExpCtr << "\n";
	ostr << "Monitor:  " << std::fixed << dMon << " of " << dSel << "\n";
	ostr << "Progress: " << std::fixed << dProgress*100. << " %";

	set_progress(int(dProgress*100.), ostr.str());
}


void disconnected(const std::string& strHost, const std::string& strSrv)
{
	log_info("Disconnected from ", strHost, " on port ", strSrv, ".");;
}

void connected(const std::string& strHost, const std::string& strSrv)
{
	log_info("Connected to ", strHost, " on port ", strSrv, ".");
}

void received(const std::string& strMsg)
{
//	log_info("Received: ", strMsg);

	std::pair<std::string, std::string> pair = split_first<std::string>(strMsg, "=", true);

	if(pair.first == "counter.Monitor 1")
		dMon = str_to_var<t_real>(pair.second);
	else if(pair.first == "counter.Counts")
		dCtr = str_to_var<t_real>(pair.second);
	else if(pair.first == "counter.Preset")
		dSel = str_to_var<t_real>(pair.second);

	refresh();
}


int main(int argc, char** argv)
{
	/*open_progress();
	set_progress(10, "abc\ndef\nghi");
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	set_progress(20, "xyz\n123\n456");
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	close_progress();
	return 0;*/

	if(argc < 5)
	{
		std::cerr << "Usage: " << argv[0] << " <server> <port> <login> <password>" << std::endl;
		return -1;
	}

	if(!open_progress())
	{
		log_err("Cannot open progress dialog.");
		return -1;
	}

	TcpClient client;
	client.add_receiver(received);
	client.add_disconnect(disconnected);
	client.add_connect(connected);


	if(!client.connect(argv[1], argv[2]))
	{
		log_err("Error: Cannot connect.");
		return -1;
	}


	std::string strLogin = argv[3];
	std::string strPwd = argv[4];
	client.write(strLogin + " " + strPwd + "\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	while(1)
	{
		client.write("counter getcounts\ncounter getmonitor 1\ncounter getpreset\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	close_progress();
	return 0;
}
