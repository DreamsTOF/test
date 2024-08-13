/*
 * @Description // 文件描述:
 * @Author: error: git config user.name & please set dead value or install git
 * @Github: https://github.com/a1798780261
 * @Date: 2024-08-01 01:34:07
 * @LastEditTime: 2024-08-04 20:44:02
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @FilePath: /test/boost_searcher/index.hpp
 * @LastEditors / 修改人员: dream
 */
#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <unordered_map>
#include "util.hpp"
#include <mutex>
namespace ns_index
{
    struct FileInfo
    {
        std::string title;   // 文档标题
        std::string content; // 文档简介
        std::string url;     // 文档在官网中的url
        uint64_t id;         // 文档id,方便构建倒排索引才需要
    };
    struct InvertedElem
    {
        uint64_t id;
        int weight;       // 权重
        std::string word; // 词
                          // 重载比较运算符，排序时就不需要使用lambda，直接使用sort即可
        bool operator<(const InvertedElem &rhs) const
        {
            return weight > rhs.weight;
        }
        bool operator>(const InvertedElem &rhs) const
        {
            return weight < rhs.weight;
        }
        bool operator==(const InvertedElem &rhs) const
        {
            return id == rhs.id;
        }
    };
    // 倒排拉链
    typedef std::vector<InvertedElem>
        InvertedList;
    struct word_cnt
    {
        int title_cnt;
        int content_cnt;
        word_cnt() : title_cnt(0), content_cnt(0) {}
    };
    struct Statistics
    {
        std::unordered_map<std::string, word_cnt> word_map;
        FileInfo *file_info;
        Statistics(FileInfo *file_info) : file_info(file_info) {}
    };
    void *TitleStatistics(void *arg)
    {
        Statistics *statistics = (Statistics *)arg;
        // 统计标题词频
        // 1.使用jieba对标题进行分词
        // 暂存在vector中
        std::vector<std::string> words;
        ns_util::JiebaUtil::CutString(statistics->file_info->title, &words);
        // 2.进行词频统计
        for (auto word : words)
        {
            boost::to_lower(word);
            statistics->word_map[word].title_cnt++;
        }
    }
    void *ContentStatistics(void *arg)
    {
        Statistics *statistics = (Statistics *)arg;
        // 统计正文词频
        // 1.使用jieba对正文进行分词
        // 暂存在vector中
        std::vector<std::string> words;
        ns_util::JiebaUtil::CutString(statistics->file_info->content, &words);

        // 2.进行词频统计
        // 使用引用会改变words内的值，所以这里不使用
        for (auto word : words)
        {
            boost::to_lower(word);
            statistics->word_map[word].content_cnt++;
        }
    }
    class Index
    {
    private:
        Index() {};
        Index(const Index &) = delete;
        Index &operator=(const Index &) = delete;
        static Index *instance_;
        static std::mutex mutex_;

    public:
        ~Index()
        {
        }
        static Index *GetInstance()
        {
            if (instance_ == nullptr)
            {
                mutex_.lock();
                if (instance_ == nullptr)
                {
                    instance_ = new Index();
                }
                mutex_.unlock();
            }
            return instance_;
        }
        // 通过ID获取文档内容
        FileInfo *GetFowardIndex(uint64_t id)
        {
            if (id >= file_infos.size())
            {
                std::cerr << "id错误" << std::endl;
                return nullptr;
            }
            return &file_infos[id];
        }
        // 根据关键字获取倒排拉链
        InvertedList *GetInvertedIndex(const std::string &keyword)
        {
            // if ()
            auto iter = inverted_index.find(keyword);
            if (iter == inverted_index.end())
            {
                std::cerr << "keyword错误" << std::endl;
                return nullptr;
            }
            return &iter->second;
        }
        // 根据去标签格式化后的文档，构建正排和倒排索引
        // data/index/raw.txt
        bool BuildIndex(const std::string &path) // 干净的数据
        {
            std::ifstream in(path, std::ios::in | std::ios::binary);
            if (!in)
            {
                std::cerr << "打开文件失败" << std::endl;
                return false;
            }
            std::string line;
            // 每次读取一行，即一个文档
            int count = 0;
            while (getline(in, line))
            {
                FileInfo *file_info = BulidForwardIndex(line);
                if (file_info == nullptr)
                {
                    std::cerr << "构建正排索引失败" << std::endl;
                    return false;
                }
                count++;
                if (count % 50 == 0)
                {
                    std::cout << "当前已经建立的索引文档: " << count << std::endl;
                }
                BulidInvertedIndex(*file_info);
            }
            return true;
        }

    private:
        FileInfo *BulidForwardIndex(const std::string &line)
        {
            // 构建正排 索引
            FileInfo file_info;
            // 1.解析line，对数据进行切分
            const static std::string sep = "\3";
            std::vector<std::string> results;
            // 字符串切分
            ns_util::StringUtil::SplitString(line, &results, sep);
            if (results.size() != 3)
            {
                std::cerr << "解析失败" << std::endl;
                return nullptr;
            }
            // 2.对数据进行赋值
            file_info.title = results[0];
            file_info.content = results[1];
            file_info.url = results[2];
            file_info.id = file_infos.size();
            file_infos.push_back(file_info);
            // 需要的是最新的一个文件，所以返回vector的最后一个元素
            return &file_infos.back();
        }
        bool BulidInvertedIndex(const FileInfo &doc)
        {
            // DocInfo{title, content, url, doc_id}
            // word -> 倒排拉链
            struct word_cnt
            {
                int title_cnt;
                int content_cnt;

                word_cnt() : title_cnt(0), content_cnt(0) {}
            };
            std::unordered_map<std::string, word_cnt> word_map; // 用来暂存词频的映射表

            // 对标题进行分词
            std::vector<std::string> title_words;
            ns_util::JiebaUtil::CutString(doc.title, &title_words);

            // if(doc.doc_id == 1572){
            //     for(auto &s : title_words){
            //         std::cout << "title: " << s << std::endl;
            //     }
            // }

            // 对标题进行词频统计
            for (std::string s : title_words)
            {
                boost::to_lower(s);      // 需要统一转化成为小写
                word_map[s].title_cnt++; // 如果存在就获取，如果不存在就新建
            }

            // 对文档内容进行分词
            std::vector<std::string> content_words;
            ns_util::JiebaUtil::CutString(doc.content, &content_words);
            // if(doc.doc_id == 1572){
            //     for(auto &s : content_words){
            //         std::cout << "content: " << s << std::endl;
            //     }
            // }

            // 对内容进行词频统计
            for (std::string s : content_words)
            {
                boost::to_lower(s);
                word_map[s].content_cnt++;
            }

#define X 10
#define Y 1
            // Hello,hello,HELLO
            for (auto &word_pair : word_map)
            {
                InvertedElem item;
                item.id = doc.id;
                item.word = word_pair.first;
                item.weight = X * word_pair.second.title_cnt + Y * word_pair.second.content_cnt; // 相关性
                InvertedList &inverted_list = inverted_index[word_pair.first];
                inverted_list.push_back(std::move(item));
            }

            return true;
        }
        //         bool BulidInvertedIndex(FileInfo &file_info)
        //         {
        //             // 构建倒排索引
        //             Statistics statistics(&file_info);

        //             // 使用多线程分别统计标题词频和正文词频
        //             pthread_t tid1, tid2;
        //             pthread_create(&tid1, nullptr, ContentStatistics, &statistics);
        //             pthread_create(&tid2, nullptr, TitleStatistics, &statistics);
        //             pthread_join(tid1, nullptr);
        //             pthread_join(tid2, nullptr);
        // #define X 10 // 标题权重比
        // #define Y 1  // 正文权重比
        //             for (auto &iter : statistics.word_map)
        //             {
        //                 InvertedElem elem;
        //                 elem.id = file_info.id;
        //                 elem.word = iter.first;
        //                 elem.weight = iter.second.title_cnt * X + iter.second.content_cnt * Y;
        //                 inverted_index[iter.first].push_back(std::move(elem));
        //             }
        //             return true;
        //         }

    private:
        // 正排索引，用数组，数组下标天然极速文档的ID
        std::vector<FileInfo>
            file_infos;
        // 倒排索引用哈希表，一个关键字可与一组InvertedElem进行对应（关键字和倒排拉链的映射关系）
        std::unordered_map<std::string, InvertedList> inverted_index;
    };
    Index *Index::instance_ = nullptr;
    std::mutex Index::mutex_;
}