/*
 * @Description // 文件描述:
 * @Author: a1798780261
 * @Github: https://github.com/a1798780261
 * @Date: 2024-08-03 06:35:34
 * @LastEditTime: 2024-08-05 02:37:18
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @FilePath: /test/boost_searcher/http_server.cc
 * @LastEditors / 修改人员: dream
 */
#include "searcher.hpp"
#include "cpp-httplib/httplib.h"
const std::string root_path = "./wwwroot";
const std::string raw = "data/raw_html/raw.txt";
int main(int argc, char *argv[])
{
    ns_searcher::Searcher searcher;
    searcher.InitSearcher(raw);
    httplib::Server svr;
    // 设置路径
    svr.set_base_dir(root_path);
    svr.Get("/hi", [](const httplib::Request &req, httplib::Response &res)
            { res.set_content("hello world", "text/plain"); });
    svr.Get("/s", [&searcher](const httplib::Request &req, httplib::Response &res)
            {
        if(!req.has_param("word"))
        {
            res.set_content("要有搜索文本", "text/plain:chatset=utf-8");
            return;
        }
        std::string word = req.get_param_value("word");
        std::cout << "正在搜索" << word << std::endl;
        std::string json_string;
        searcher.Searche(word, &json_string);
        res.set_content(json_string, "application/json"); });
    svr.listen("0.0.0.0", 8080);

    return 0;
}