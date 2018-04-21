/*
 * Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
 *
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Nicklas P Andersson
 */

#include "../StdAfx.h"

#include "AMCPCommand.h"
#include "AMCPCommandQueue.h"
#include "AMCPCommandScheduler.h"

namespace caspar { namespace protocol { namespace amcp {

class AMCPScheduledCommand
{
  private:
    core::frame_timecode                                 timecode_;
    std::map<std::wstring, std::shared_ptr<AMCPCommand>> commands_{};

  public:
    AMCPScheduledCommand(const std::shared_ptr<AMCPCommand> command, core::frame_timecode timecode, std::wstring token)
        : timecode_(timecode)
    {
        commands_.insert({std::move(token), std::move(command)});
    }

    void add(const std::wstring& token, std::shared_ptr<AMCPCommand> command) { commands_.insert({token, command}); }

    bool try_pop_token(const std::wstring& token)
    {
        const auto v   = commands_.find(token);
        const bool res = v != commands_.end();

        if (res)
            commands_.erase(v);

        return res;
    }

    std::shared_ptr<AMCPGroupCommand> create_command() const
    {
        std::vector<std::shared_ptr<AMCPCommand>> cmds;
        for (const auto cmd : commands_) {
            cmds.push_back(cmd.second);
        }

        return std::move(std::make_shared<AMCPGroupCommand>(cmds));
    }

    std::vector<std::pair<core::frame_timecode, std::wstring>> get_tokens()
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        for (const auto cmd : commands_) {
            res.push_back({timecode_, cmd.first});
        }

        return res;
    }

    std::shared_ptr<AMCPCommand> find(const std::wstring& token)
    {
        const auto it = commands_.find(token);
        if (it == commands_.end())
            return nullptr;

        return it->second;
    }

    core::frame_timecode timecode() const { return timecode_; }
};

class AMCPCommandSchedulerQueue
{
  public:
    AMCPCommandSchedulerQueue()
        : scheduled_commands_()
        , last_timecode_(core::frame_timecode::empty())
    {
    }

    void set(const std::wstring& token, const core::frame_timecode& timecode, std::shared_ptr<AMCPCommand> command)
    {
        if (!command || token.empty() || timecode == core::frame_timecode::empty())
            return;

        for (auto cmd : scheduled_commands_) {
            if (cmd->timecode() != timecode)
                continue;

            cmd->add(token, command);
            return;
        }

        // No match, so queue command instead
        auto sch_cmd = std::make_shared<AMCPScheduledCommand>(command, timecode, token);
        scheduled_commands_.push_back(std::move(sch_cmd));
    }

    bool remove(const std::wstring& token)
    {
        if (token.empty())
            return false;

        for (auto cmd = scheduled_commands_.begin(); cmd != scheduled_commands_.end(); ++cmd) {
            if (!cmd->get()->try_pop_token(token))
                continue;

            // Discard slot if now empty
            if (cmd->get()->get_tokens().size() == 0) {
                scheduled_commands_.erase(cmd);
            }

            return true;
        }

        return false;
    }

    void clear() { scheduled_commands_.clear(); }

    std::vector<std::pair<core::frame_timecode, std::wstring>> list(core::frame_timecode& timecode)
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        const bool include_all = timecode == core::frame_timecode::empty();

        for (auto command : scheduled_commands_) {
            for (auto token : command->get_tokens()) {
                if (include_all || timecode == token.first)
                    res.push_back(std::move(token));
            }
        }

        return res;
    }

    std::pair<core::frame_timecode, std::shared_ptr<AMCPCommand>> find(const std::wstring& token)
    {
        for (auto command : scheduled_commands_) {
            const auto cmd = command->find(token);
            if (cmd)
                return std::make_pair(command->timecode(), cmd);
        }

        return std::make_pair(core::frame_timecode::empty(), nullptr);
    }

    std::pair<core::frame_timecode, core::frame_timecode> find_range(const core::frame_timecode& timecode) const
    {
        // if fps mismatch, then go with one frame
        if (last_timecode_.fps() != timecode.fps())
            return std::make_pair(timecode, timecode + 1);
        
        return std::make_pair(last_timecode_, timecode + 1);
    }

    std::vector<std::shared_ptr<AMCPGroupCommand>> schedule(const core::frame_timecode& timecode)
    {
        std::vector<std::shared_ptr<AMCPGroupCommand>> res;

        if (scheduled_commands_.size() == 0) {
            last_timecode_ = timecode;
            return res;
        }

        const std::pair<core::frame_timecode, core::frame_timecode> range = find_range(timecode);
        last_timecode_                                                    = timecode;

        for (int i = 0; i < scheduled_commands_.size(); i++) {
            const auto cmd = scheduled_commands_[i];
            if (cmd->timecode().is_between(range.first, range.second)) {
                res.push_back(std::move(cmd->create_command()));
                scheduled_commands_.erase(scheduled_commands_.begin() + i);
                --i;
            }
        }

        return res;
    }

  private:
    std::vector<std::shared_ptr<AMCPScheduledCommand>> scheduled_commands_;
    core::frame_timecode                               last_timecode_;
};

struct AMCPCommandScheduler::Impl
{
  private:
    std::vector<std::shared_ptr<AMCPCommandSchedulerQueue>> queues_;
    std::timed_mutex                                        lock_;

  public:
    void add_channel() { queues_.push_back(std::make_shared<AMCPCommandSchedulerQueue>()); }

    void set(int                          channel_index,
             const std::wstring&          token,
             const core::frame_timecode&  timecode,
             std::shared_ptr<AMCPCommand> command)
    {
        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            queue->remove(token);
        }

        queues_.at(channel_index)->set(token, timecode, std::move(command));
    }

    bool remove(const std::wstring& token)
    {
        if (token.empty())
            return false;

        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            if (queue->remove(token))
                return true;
        }

        return false;
    }

    void clear()
    {
        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            queue->clear();
        }
    }

    std::vector<std::pair<core::frame_timecode, std::wstring>> list(core::frame_timecode& timecode)
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            for (auto token : queue->list(timecode)) {
                res.push_back(std::move(token));
            }
        }

        return res;
    }

    std::pair<core::frame_timecode, std::shared_ptr<AMCPCommand>> find(const std::wstring& token)
    {
        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            const auto res = queue->find(token);
            if (res.first != core::frame_timecode::empty())
                return res;
        }

        return std::make_pair(core::frame_timecode::empty(), nullptr);
    }

    class timeout_lock
    {
      private:
        std::timed_mutex& mutex_;
        bool              locked_;

      public:
        timeout_lock(std::timed_mutex& mutex, int milliseconds)
            : mutex_(mutex)
        {
            locked_ = mutex_.try_lock_for(std::chrono::milliseconds(milliseconds));
        }
        ~timeout_lock() { mutex_.unlock(); }

        bool is_locked() const { return locked_; }
    };

    boost::optional<std::vector<std::shared_ptr<AMCPGroupCommand>>> schedule(int                         channel_index,
                                                                             const core::frame_timecode& timecode)
    {
        // TODO - tweak this timeout
        // Timeout on lock aquiring. This is to stop channel output stalling
        timeout_lock lock(lock_, 5);
        if (!lock.is_locked())
            return boost::none;

        return queues_.at(channel_index)->schedule(timecode);
    }
};

AMCPCommandScheduler::AMCPCommandScheduler()
    : impl_(new Impl())
{
}

void AMCPCommandScheduler::add_channel() { impl_->add_channel(); }

void AMCPCommandScheduler::set(int                          channel_index,
                               const std::wstring&          token,
                               const core::frame_timecode&  timecode,
                               std::shared_ptr<AMCPCommand> command)
{
    impl_->set(channel_index, token, timecode, command);
}

bool AMCPCommandScheduler::remove(const std::wstring& token) { return impl_->remove(token); }

void AMCPCommandScheduler::clear() { return impl_->clear(); }

std::vector<std::pair<core::frame_timecode, std::wstring>> AMCPCommandScheduler::list(core::frame_timecode& timecode)
{
    return impl_->list(timecode);
}

boost::optional<std::vector<std::shared_ptr<AMCPGroupCommand>>>
AMCPCommandScheduler::schedule(int channel_index, core::frame_timecode timecode)
{
    return impl_->schedule(channel_index, timecode);
}

std::pair<core::frame_timecode, std::shared_ptr<AMCPCommand>> AMCPCommandScheduler::find(const std::wstring& token)
{
    return impl_->find(token);
}

}}} // namespace caspar::protocol::amcp
