#include "httplib.h"
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <fstream>
#include <sys/types.h>
#include <boost/thread.hpp>
#include <ifaddrs.h>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#define SHARED_DIR "shared"
using httplib::Request;
using httplib::Response;
using std::cout;
using std::endl;
using std::cerr;
using std::string;
namespace bf = boost::filesystem;
#define DOWNLOAD_DIR "download"
#define RANGE_SIZE (1024 * 1024 * 100)

#define __DEBUG__
//#define __RELEASE__
class P2PClient
{
  private:
    boost::mutex _mtx;
    httplib::Client _clt;
    uint16_t _srv_port;
    std::string _host;
    std::vector<std::string> _host_list;
    std::vector<std::string> _online_host;
    std::vector<std::string> _file_list;
  private:
    //获取主机列表
    bool GetHostList()
    {
      if(!_host_list.empty())
      {
        _host_list.clear();
      }
      struct ifaddrs *addrs = NULL; 
      struct sockaddr_in *ip = NULL;
      struct sockaddr_in *mask = NULL;
      getifaddrs(&addrs);
      while(addrs != NULL)
      {
        ip = (struct sockaddr_in*)addrs->ifa_addr;
        mask = (struct sockaddr_in*)addrs->ifa_netmask;
        if(ip->sin_family != AF_INET)
        {
          addrs = addrs->ifa_next;
          continue;
        }
        if(ip->sin_addr.s_addr == inet_addr("127.0.0.1"))
        {
          addrs = addrs->ifa_next;
          continue;
        }
        //printf("name:%s\n", addrs->ifa_name);
        //printf("ip:%s\n", inet_ntoa(ip->sin_addr));
        //printf("mask:%s\n", inet_ntoa(mask->sin_addr));

        uint32_t net = ntohl(ip->sin_addr.s_addr & mask->sin_addr.s_addr);
        uint32_t host = ntohl(~mask->sin_addr.s_addr);
        uint32_t i;
        for(i = 1; i < host; ++i)
        {
          struct in_addr ip;
          ip.s_addr = htonl(net + i);
          //printf("ip:%s\n", inet_ntoa(ip));
          //把可能在线的ip存入主机列表中
          _host_list.push_back(inet_ntoa(ip));
        }
        break;
      }
      return true;
    }
    //获取文件大小
    int64_t GetFileSize(unsigned long file_id)
    {
      if(file_id < 0 || file_id > _file_list.size())
      {
        cerr << "file_id is errno" << endl ;
      }
      string file_name = _file_list[file_id];
      string reqPath = "/list/";
      reqPath += file_name;
      auto res = _clt.Head(reqPath.c_str());
      auto size = res->get_header_value("Content-Length"); 
      ino64_t ret;
      std::stringstream ss;
      ss << size;
      ss >> ret; 
      return ret;
    }
    //线程执行的单个主机配对
    void PairHost(const std::string ip)
    {
      httplib::Client client(ip.c_str(), _srv_port); 
      auto res = client.Get("/hostpair");
      if(!res)
      {
        return;
      }
      if(res->status == 200) 
      {
        cout << ip << " : success" << endl;
        _online_host.push_back(ip); 
      }
    }

    int DoFace()
    {
      cout << "1. 匹配附近主机" << endl;
      cout << "2. 显示在线主机" << endl;
      cout << "0. 退出" << endl;
      int choose;
      std::cin >> choose;
      return choose;
    }

    void RangeDownLoad(std::string name, int64_t start, int64_t end, bool *res)
    {
      std::string url = "/list/" + name; 
      std::string real_path; 
      std::stringstream ss;
      ss << DOWNLOAD_DIR << "/" << name;
      ss >> real_path;
      std::stringstream range_val;
      range_val << "bytes=" << start << "-" << end;

      cout << _host << endl;//log..
      httplib::Client client(_host.c_str(), _srv_port);
      httplib::Headers hdr;
      hdr.insert(std::make_pair("Range", range_val.str().c_str()));
      auto rsp = client.Get(url.c_str(), hdr);
      if(rsp && rsp->status == 206)
      {
#ifdef __DEBUG__ 
        cout << "[RangeDownLoad]";
        cout << "rsp->status = 206" << endl;
        cout << "read_path = " << real_path << endl;
#endif
        if(!bf::exists(DOWNLOAD_DIR))
        {
          bf::create_directory(DOWNLOAD_DIR);
        }
        std::ofstream file(real_path, std::ios::binary);
        if(!file.is_open())
        {
          cerr << "file " << real_path << " open erro" << endl;
          *res = false;
          return;
        }
#ifdef __DEBUG__ 
        cout << "[RangeDownLoad]";
        cout << "rsp->body.size() = " << rsp->body.size() << endl;
#endif
        _mtx.lock();
        file.seekp(start, std::ios::beg);
        file.write(&rsp->body[0], rsp->body.size());
        _mtx.unlock();
        if(!file.good())
        {
          cerr << "file " << real_path << " write erro" << endl;
          file.close();
          *res = false;
          return;
        }
        *res = true;
#ifdef __DEBUG__ 
        cout << "[RangeDownLoad]";
        cout << "res = " << res << endl;
#endif
        file.close();
      }
      else
      {
        cout << "rsp->status = "  << rsp->status<< endl;
      }
      return;
    }
  public:
    P2PClient(std::string ip, int port)
      :_clt(ip.c_str(), port){_srv_port = port;}
    int SelectShow()//用户选择显示
    {
      while(1)
      {
        int choose = DoFace();
        if(choose == 1)
        {
          PairNearbyHost();
          continue;
        }
        else if(choose == 2)
        {
          if(_online_host.empty())
          {
            cout << "暂无主机在线，请稍后尝试配对" << endl;
            continue;
          }
          ShowOnlienHost();
          cout << "请选择你要查看的主机：";
          int choice;
          std::cin >> choice;
          if(choice != 0)
          {
            GetShareList(choice - 1);
            while(1)
            {
              ShowShareList();
              if(_file_list.empty())
              {
                break;
              }
              cout << "请选择你要下载的文件:";
              std::cin >> choice;
              if(choice != 0)
              {
                DownLoadFile(choice); 
              }
              else 
              {
                break;
              }
            }
          }
          else 
          {
            continue;
          }
        }
        else 
        {
          exit(0);
        }
      }
      return 0;
    }
    bool PairNearbyHost() //附近主机匹配
    {
#ifdef __DEBUG__ 
      _online_host.push_back("192.168.0.172");
#endif
#ifdef __RELEASE__
      GetHostList();
      uint32_t i;
      _online_host.clear();
      std::vector<boost::thread> thd_list;
      thd_list.resize(_host_list.size());
      for(i = 0; i < _host_list.size(); ++i)
      {
        std::string ip = _host_list[i];
        boost::thread thd(&P2PClient::PairHost, this, ip);
        thd_list[i] = std::move(thd); 
      }
      for(i = 0; i < thd_list.size(); ++i)
      {
        thd_list[i].join();
      }
      return true;
#endif
    }
    bool ShowOnlienHost() //显示附近在线主机
    {
      if(_online_host.empty())
      {
        return false;
      }
      uint32_t i;
      for(i = 0; i < _online_host.size(); ++i)
      {
        cout << i + 1 << ": " << _online_host[i] << endl;
      }
      return true;
    }
    bool GetShareList(int host_id) //获取指定主机的共享文件列表
    {
      //_srv.Get("/list", ListHandler);
      if(host_id < 0 || host_id > (int)_online_host.size())
      {
        return false;
      }
      _host = _online_host[host_id];
      _file_list.clear(); 
      httplib::Client client(_online_host[host_id].c_str(), _srv_port);
      auto res = client.Get("/list");
      if(res && res->status == 200)
      {
        boost::split(_file_list, res->body, boost::is_any_of("\n"));
      }
      return true;
    }
    bool ShowShareList() //显示指定主机的共享文件列表
    {
      if(_file_list.empty()) 
      {
        cout << "无共享文件" << endl;
        return false;
      }
      for(uint32_t i = 0; i < _file_list.size() - 1; ++i)
      {
        cout << i + 1 << " : " << _file_list[i] << endl;
      }
      return true;
    }
    bool DownLoadFile(int file_id) //下载指定主机的共享文件
    {
      if(file_id < 0 || file_id > (int)_file_list.size())
      {
        cerr << "您的输入有误" << endl;
        return false;
      }
      //获取文件大小
      int64_t size = GetFileSize(file_id - 1);
      cout << size << endl;//log...
      if(size <= 0)
      {
        cout << "空文件" << endl; 
        return false;
      }
      int Count = size / RANGE_SIZE;
#ifdef __DEBUG__
        cout << "[DownLoadFile]";
        cout << "Count = " << Count<< endl;
#endif
      std::string name = _file_list[file_id - 1];
      std::vector<boost::thread> thd_list;
      thd_list.resize(Count + 1);
      std::vector<bool> res_list;
      res_list.resize(Count + 1);
      for(int i = 0; i <= Count; ++i)
      {
        int64_t start, end, rlen;
        start = i * RANGE_SIZE;
        end = (i + 1) * RANGE_SIZE - 1;
        if(i == Count)
        {
          if(size % RANGE_SIZE == 0)
          {
            break;
          }
          end = size - 1;
        }
        rlen = end - start - 1;
        //void RangeDownLoad(std::string host, std::string name, int64_t start, int64_t end)
        bool res = res_list[i]; 
#ifdef __DEBUG__
        ///cout << "[DownLoadFile]";
        ///cout << "res " << res << endl;
#endif
        boost::thread thd(&P2PClient::RangeDownLoad, this, name, start, end, &res);
        //thd.join();
        thd_list[i] = std::move(thd);
      }
      bool ret = true;
      for(int i = 0; i <= Count; ++i)
      {
        if(i == Count && size % RANGE_SIZE == 0)
        {
          break;
        }
        thd_list[i].join();
        if(res_list[i] == false)
        {
          ret = false;
        }
      }
      if(ret == true)
      {
        std::cerr << "DownLoadFile " << name << " success" << endl;
      }
      if(ret == false)
      {
        std::cerr << "DownLoadFile " << name << " failed" << endl;
      }
      return true;
    }

};
int main()
{
  P2PClient c("0.0.0.0", 8000);
  c.SelectShow();
  //c.PairNearbyHost();
  //cout << "daw" << endl;
  return 0;
}
