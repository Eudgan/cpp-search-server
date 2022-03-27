#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_{search_server}
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> answer = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(answer);
    return answer;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
 return count_if(requests_.begin(), requests_.end(),
             [](const QueryResult& i)    {return !i.result;});
}

void RequestQueue::AddRequest(const std::vector<Document>& answer) {
    ++count_;
    if (requests_.size() >= min_in_day_) {
           requests_.pop_front();
           --count_;
    }
        requests_.push_back({count_, !answer.empty()});
}
