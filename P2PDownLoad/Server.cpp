#include "httplib.h"
#include <fstream>
#include <boost/thread.hpp>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#define SHARED_DIR "shared"
#define _SRV_PORT 8000
#define __DEBUG__
using httplib::Request;
using httplib::Response;
using std::cout;
using std::endl;
using std::string;
namespace bf = boost::filesystem;
class P2PServer
{
  private:
    httplib::Server _srv;
  public:
    /*附近主机配对请求处理*/
    static void PairHandler(const Request& req, Response& res)
    {
      res.status = 200;
      cout << "Request" << endl;
    }
    /*文件列表请求处理*/
    static void ListHandler(const Request& req, Response& res)
    {
      //如果目录不存在
      res.status = 200;
      if(!bf::exists(SHARED_DIR)) 
      {
        //则创建
        bf::create_directory(SHARED_DIR);
      }
      bf::directory_iterator file_begin(SHARED_DIR);
      bf::directory_iterator file_end;
      for(auto i = file_begin; i != file_end; ++i)
      {
        if(!bf::is_directory(i->status()))
        {
          auto tmp = i->path().filename();
          res.body += tmp.c_str();
          res.body += "\n";
          cout << tmp.c_str() << endl;
        }
      }
    }
    /*文件下载请求处理*/
    static void DownloadHandler(const Request& req, Response& res)
    {
      bf::path path(req.path);
      //打开文件
      std::string name = path.filename().c_str();
      std::string real_path = SHARED_DIR;
      real_path += "/";
      real_path += name;
      cout << "real_path = " << real_path << endl;
      std::ifstream file(real_path, std::ios::binary); 
      if(!file.is_open())
      {
        std::cerr << "file open failed" << endl;
      }
      //计算文件大小
      int64_t size = bf::file_size(real_path.c_str());
      string s_size = std::to_string(size);
      if(req.method == "HEAD")
      {
        res.status = 200;
#ifdef __DEBUG__ 
        cout << "[DownloadHandler]";
        cout << "s_size = " << s_size << endl;//log...
#endif
        res.set_header("Content-Length", s_size.c_str()); 
        return;
      }
      else 
      {
        cout << "请求下载" << endl;
        if(!req.has_header("Range"))
        {
          res.status = 400;
          return;
        }
        res.status = 206;
        std::string range_val = req.get_header_value("Range");
        int64_t start, end, rlen;
        size_t pos1 = range_val.find("=");
        size_t pos2 = range_val.find("-");
        if(pos1 == std::string::npos || pos2 == std::string::npos)
        {
          res.status = 400;
          std::cerr << "range " << range_val << " format error" << endl;
          return;
        }
#ifdef __DEBUG__
          cout << "[DownloadHandler]";
          std::cerr << "range " << range_val << endl;
#endif
        std::string rstart;
        std::string rend;
        rstart = range_val.substr(pos1 + 1, pos2 - pos1 - 1);
        rend = range_val.substr(pos2 + 1);
        std::stringstream tmp;
        tmp << rstart;
        tmp >> start;
        tmp.clear();
        tmp << rend;
        tmp >> end;
        std::ifstream file(real_path.c_str(), std::ios::binary);
        if(!file.is_open())
        {
          std::cerr << "open file " << real_path  << " failed" << endl;
        }
        rlen = end - start;
#ifdef __DEBUG__
          cout << "[DownloadHandler]";
          printf("%ld = %ld - %ld\n", rlen, end, start);//log...
#endif
          res.body.resize(rlen + 1);
          file.seekg(start, std::ios::beg);
          file.read(&res.body[0], res.body.size());
        if(!file.good())
        {
          std::cerr << "read file " << real_path << " body erro" << endl;
          res.status = 500;
          return;
        }
      }
    }
    //static void GetFileSize(const Request &req, Response &res)
    //{
    //  
    //  res.status = 200;
    //  res.set_header("Content-Length", s_size.c_str());
    //  cout << res.get_header_value("Content-Length") << endl;
    //  cout << "s_size = " << s_size << endl;
    //}

    //static void TestReq(const Request &req, Response &res)
    //{
    //  cout << "req.path: " << req.path<< endl;
    //  cout << "req.body: " << req.body << endl;
    //  auto tmp = req.headers;
    //  for(auto &i : tmp)
    //  {
    //    cout << "headers.first headers.second = " << i.first << " " << i.second << endl;
    //  }
    //  auto tmp2 = req.matches;
    //  for(auto &i : tmp2)
    //  {
    //    cout << "matches.str = " << i.str() << endl;
    //  }
    //  auto tmp3 = req.method; 
    //  cout << "mothod.c_str = " << tmp3.c_str() << endl;
    //  auto tmp4 = req.params;
    //  for(auto &i : tmp4)
    //  {
    //    cout << "params.first params.second = " << i.first << " " << i.second << endl;
    //  }
    //  auto tmp5 = req.target;
    //  cout << "target.c_str = " << tmp5.c_str() << endl;
    //  auto tmp6 = req.version;
    //  cout << "version.c_str = " << tmp6.c_str() << endl;
    //}

    bool Start(uint16_t port)
    {
      _srv.Get("/hostpair", PairHandler); 
      _srv.Get("/list", ListHandler); 
      _srv.Get("/list(.*)", DownloadHandler); 
      //_srv.Get("/list(.*)", GetFileSize); 
      _srv.listen("0.0.0.0", port);
      return true;
    }

};

int main()
{
  P2PServer s;
  s.Start(_SRV_PORT);
  return 0;
}
