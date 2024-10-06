#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <cpr/cpr.h>
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include <nlohmann/json.hpp>
using json = nlohmann::json;

std::vector<std::string> getNSESecuritites(int retryCount = 5){
    for(int i=0; i<retryCount; i++){
        cpr::Response res = cpr::Get(cpr::Url{"https://archives.nseindia.com/content/equities/EQUITY_L.csv"});
        if(res.status_code == 200){
            spdlog::debug("Received symbol data from NSE");
            long i=0;
            // Skip 1st line
            while(res.text[i] != '\n'){
                i++;
            }
            i++;
            std::vector<std::string> symbols;
            while(i < res.text.length()){
                std::string curr;
                // Get symbol
                while(res.text[i] != ','){
                    curr.push_back(res.text[i]);
                    i++;
                }

                // Skip the rest
                while(res.text[i] != '\n'){
                    i++;
                }
                i++;
                symbols.push_back(std::move(curr));
            }
            return symbols;
        }
        spdlog::warn("Received Status Code : {} and Error msg : {} while loading from  .. Retrying", res.status_code, res.error.message);
    }
    spdlog::warn("Fetching Symbols from NSE failed.");
    return {};
}

void fetchFromMotilal(std::string &symbol, int retryCount=5){
    std::string url{"https://research360api.motilaloswal.com/api/getcharts/history?symbol="};
    static std::string url_query{"&resolution=1&from=1691790092&to=1691856554&countback=300&currencyCode=INR"};
    url.append(symbol);
    url.append(url_query);
    for(int i=0; i<retryCount; i++){
        cpr::Response res = cpr::Get(cpr::Url{url}, cpr::Header{{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36"}, {"Origin", "https://www.research360.in"}, {"Referer","https://www.research360.in/"}});
        if(res.status_code == 200){
            spdlog::debug("Successfully fetched data of symbol : {}", symbol);
            std::ofstream f("data/"+symbol+".json");
            f << res.text;
            f.close();
            return;
        }else if(res.status_code == 404){
            spdlog::critical("Fetching data of symbol failed : {}", symbol);
            return;
        }
        spdlog::warn("Fetching symbol failed : {} , Retrying...", symbol);
    }

}

int main(int argc, char** argv) {
    // SPDLOG_LEVEL=info,mylogger=trace && ./example
    spdlog::cfg::load_env_levels();

    std::vector<std::string> securities = getNSESecuritites();
    
    if (!std::filesystem::is_directory("data") || !std::filesystem::exists("data")) {
        std::filesystem::create_directory("data");
    }

    std::mutex m;
    std::queue<std::string> q;

    for(auto &s: securities){
        q.push(s);
    }

    std::vector<std::thread> threads;

    // 10 threads
    for(int i=0; i<10; i++){
        threads.push_back(std::thread([&m, &q](){
            while(true){
                std::unique_lock<std::mutex> lg(m);
                if(q.size() == 0){
                    break;
                }
                std::string symbol = q.front();
                q.pop();
                lg.unlock();
                fetchFromMotilal(symbol);
            }
            return;
        }));
    }

    for(auto &t: threads){
        t.join();
    }
    return 0;
}
