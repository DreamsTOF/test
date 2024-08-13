/*
 * @Description // 文件描述:
 * @Author: a1798780261
 * @Github: https://github.com/a1798780261
 * @Date: 2024-08-02 05:32:07
 * @LastEditTime: 2024-08-05 04:48:46
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @FilePath: /test/boost_searcher/searcher.hpp
 * @LastEditors / 修改人员: dream
 */

#ifndef SEARCHER_A9C7DC5A_81E3_4240_9073_1129F0CD7300
#define SEARCHER_A9C7DC5A_81E3_4240_9073_1129F0CD7300
#include <unordered_set> //去重
#include "index.hpp"
#include "util.hpp"
#include <algorithm> //去重
#include <set>
#include <jsoncpp/json/json.h>
namespace ns_searcher
{
    std::string GetDesc(std::string &content, std::string &word);
    class Searcher
    {
    public:
        Searcher() {}
        ~Searcher() {}
        // 初始化
        void InitSearcher(const std::string &input)
        {
            // 1. 获取或者创建index
            index_ = ns_index::Index::GetInstance();
            std::cout << "获取单例成功" << std::endl;
            // 构建索引需要路径
            index_->BuildIndex(input);
            // 2. 根据index创建searcher索引
            std::cout << "索引建立成功" << std::endl;
        }
        // 查找函数，用户查找调用这个，输入query，返回给浏览器json_string
        void Searche(const std::string &query, std::string *json_string)
        {
            // 1.[分词]将query按照searcher的要求进行分词
            std::vector<std::string> words;
            ns_util::JiebaUtil::CutString(query, &words);
            // 2.[触发] 根据分词的“词”，进行index查找
            std::vector<ns_index::InvertedElem> inverted_elems;
            for (auto word : words)
            {
                boost::to_lower(word);
                ns_index::InvertedList *inverted_list = index_->GetInvertedIndex(word);
                // 如果找不到
                if (inverted_list == nullptr)
                {
                    std::cout << "没有找到" << word << std::endl;
                    continue;
                }
                // 将inverted_list中的结果全部添加到inverted_elems中
                inverted_elems.insert(inverted_elems.end(), inverted_list->begin(), inverted_list->end());
            }
            // 3.[合并排序]将查找结果汇总，并按照相关性进行倒排
            // 去重方法1：将vector元素插入到unordered_set中，再插回vector进行排序，效率低下
            // 去重方法2：将vector元素插入到set中，再插回vector，set底层基于红黑树，会自动排序，效率高 O(m log m) + O(m)
            // std::set<ns_index::InvertedElem> inverted_elems_set;
            // std::for_each(inverted_elems.begin(), inverted_elems.end(), [&inverted_elems_set](const ns_index::InvertedElem &elem)
            //               { inverted_elems_set.insert(elem); });
            // std::for_each(inverted_elems_set.begin(), inverted_elems_set.end(), [&inverted_elems](const ns_index::InvertedElem &elem)
            //               { inverted_elems.push_back(elem); });
            // 去重方法3，先使用sort排序，再使用unique去重，效率更高（本质是对于随机数，快排比堆排更快） O(m log m) + O(m)
            // 去重方法4，使用计数排序，再使用unique去重，效率更高（在使用int类型进行比较，且int值通常较小时，比较快） O(m+n（max）)
            // std::unordered_set<ns_index::InvertedElem> inverted_elems_set;
            // std::for_each(inverted_elems.begin(), inverted_elems.end(), [&inverted_elems_set](const ns_index::InvertedElem &elem)
            //               { inverted_elems_set.insert(elem); });
            // std::for_each(inverted_elems_set.begin(), inverted_elems_set.end(), [&inverted_elems](const ns_index::InvertedElem &elem)
            //               { inverted_elems.push_back(elem); });
            //[](const ns_index::InvertedElem &a, const ns_index::InvertedElem &b)：这是一个 lambda 函数，它接受两个 ns_index::InvertedElem 类型的引用作为参数。
            //[&inverted_elems]则表示需要lambda 函数需要捕获的外部变量。在for_each 循环中，lambda 函数被调用，需要捕获变量，同时需要被捕获的变量也可通过参数列表传递给lambda 函数，如： [](const InvertedElem &elem, std::vector<ns_index::InvertedElem> &inverted_elems)，二者无多大区别
            // return a.weight > b.weight是lambda 函数的主体，它比较两个元素 a 和 b 的 weight 成员。如果 a.weight 大于 b.weight，则返回 true，否则返回 false。
            // unique 将重复的元素放到容器末尾，然后返回新的末尾迭代器
            // unique 的第二个参数是判断是否相等的函数，这里判断的是 id 是否相等
            auto last = std::unique(inverted_elems.begin(), inverted_elems.end(), [](const ns_index::InvertedElem &a, const ns_index::InvertedElem &b)
                                    { return a.id == b.id; });
            std::sort(inverted_elems.begin(), last, [](const ns_index::InvertedElem &a, const ns_index::InvertedElem &b)
                      { return a.weight > b.weight; });
            // 去重
            inverted_elems.erase(last, inverted_elems.end());
            // std::cout << "文本大小为：" << inverted_elems.size() << std::endl;
            // 4.[返回]将查找出的所有结果整合为json串返回给前端
            Json::Value root;
            for (auto &elem : inverted_elems)
            {
                ns_index::FileInfo *file_info = index_->GetFowardIndex(elem.id);
                if (file_info == nullptr)
                    continue;
                Json::Value item;
                item["title"] = file_info->title;
                item["content"] = GetDesc(file_info->content, elem.word);
                item["url"] = file_info->url;
                item["id"] = (int)elem.id;
                item["weight"] = elem.weight;
                root.append(item);
            }
            Json::FastWriter writer;
            *json_string = writer.write(root);
            //*json_string = root.toStyledString();
        }

    private:
        ns_index::Index *index_;
    };
    std::string GetDesc(std::string &content, std::string &word)
    {
        const int prec_step = 50;
        const int next_step = 100;
        auto iter = search(content.begin(), content.end(), word.begin(), word.end(), [](int x, int y)
                           { return std::tolower(x) == std::tolower(y); });
        int pos = std::distance(content.begin(), iter);
        if (pos != std::string::npos)
        {
            int begin = pos - prec_step;
            int end = pos + word.size() + next_step;
            if (begin < 0)
                begin = 0;
            if (end > content.size())
                end = content.size() - 1;
            if (begin >= end)
                return "None";
            return content.substr(begin, end - begin);
        }
    }
}

#endif // SEARCHER_A9C7DC5A_81E3_4240_9073_1129F0CD7300
