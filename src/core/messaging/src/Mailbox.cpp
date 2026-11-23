#include <astro/core/domain/Core.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace astro {
void Mailbox::send(Command c) {
    std::lock_guard lock(mutex_);
    commands_.push_back(std::move(c));
}
std::vector<Command> Mailbox::take() {
    std::lock_guard lock(mutex_);
    std::vector<Command> out;
    out.swap(commands_);
    return out;
}
Snapshot Mailbox::snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}
void Mailbox::publish(const Snapshot& s) {
    std::lock_guard lock(mutex_);
    snapshot_ = s;
}
std::uint64_t Mailbox::submit(Command c) {
    std::lock_guard lock(mutex_);
    if (commands_.size() >= 64) throw std::runtime_error("Camera command queue is full");
    c.id = ++nextId_;
    results_[c.id] = {c.id, false, {}};
    while (results_.size() > 512 && results_.begin()->second.finished)
        results_.erase(results_.begin());
    commands_.push_back(std::move(c));
    return nextId_;
}
void Mailbox::complete(std::uint64_t id, const std::string& error) {
    if (!id) return;
    std::lock_guard lock(mutex_);
    results_[id] = {id, true, error};
}
std::optional<CommandResult> Mailbox::result(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    const auto it = results_.find(id);
    if (it == results_.end()) return {};
    return it->second;
}

}
