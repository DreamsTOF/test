/*
 * @Description // 文件描述:
 * @Author: a1798780261
 * @Github: https://github.com/a1798780261
 * @Date: 2024-08-02 09:08:45
 * @LastEditTime: 2024-08-03 06:36:44
 * @LastEditors: a1798780261 1798780261@qq.com
 * @FilePath: /boost_searcher/server.cc
 * @LastEditors / 修改人员: dream
 */
#include "searcher.hpp"
const std::string raw = "data/raw_html/raw.txt";
int main()
{
    ns_searcher::Searcher *searcher = new ns_searcher::Searcher();
    searcher->InitSearcher(raw);
    std::string query;
    std::string json_string;
    while (true)
    {
        std::cout << "请输入查询内容: ";
        std::getline(std::cin, query);
        searcher->Searche(query, &json_string);
        std::cout << json_string << std::endl;
    }
    return 0;
}
