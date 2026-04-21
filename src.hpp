#pragma once

#include "Task.hpp"
#include <vector>

class TimingWheel;

class TaskNode {
    friend class TimingWheel;
    friend class Timer;
public:
    TaskNode(Task* t, size_t trigger_time) 
        : task(t), next(nullptr), prev(nullptr), wheel(nullptr), slot(0), trigger_time(trigger_time) {
        time = (int)trigger_time;
    }
private:
    Task* task;
    TaskNode* next, *prev;
    int time; 
    size_t trigger_time;
    TimingWheel* wheel;
    size_t slot;
};

class TimingWheel {
    friend class Timer;
public:
    TimingWheel(size_t size, size_t interval) : size(size), interval(interval) {
        slots = new TaskNode*[size];
        for (size_t i = 0; i < size; ++i) slots[i] = nullptr;
    }
    ~TimingWheel() {
        for (size_t i = 0; i < size; ++i) {
            TaskNode* curr = slots[i];
            while (curr) {
                TaskNode* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        delete[] slots;
    }
    
    void addNode(TaskNode* node) {
        size_t s = node->slot;
        node->next = slots[s];
        node->prev = nullptr;
        if (slots[s]) slots[s]->prev = node;
        slots[s] = node;
        node->wheel = this;
    }

    void removeNode(TaskNode* node) {
        if (node->prev) node->prev->next = node->next;
        else slots[node->slot] = node->next;
        if (node->next) node->next->prev = node->prev;
        node->next = node->prev = nullptr;
        node->wheel = nullptr;
    }

private:
    const size_t size, interval;
    TaskNode** slots;
};

class Timer {
public:    
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    Timer() {
        secondsWheel = new TimingWheel(60, 1);
        minutesWheel = new TimingWheel(60, 60);
        hoursWheel = new TimingWheel(24, 3600);
        current_time = 0;
    }

    ~Timer() {
        delete secondsWheel;
        delete minutesWheel;
        delete hoursWheel;
    }

    TaskNode* addTask(Task* task) {
        TaskNode* node = new TaskNode(task, current_time + task->getFirstInterval());
        addNodeToAppropriateWheel(node);
        return node;
    }

    void cancelTask(TaskNode *p) {
        if (p && p->wheel) {
            p->wheel->removeNode(p);
            delete p;
        }
    }

    std::vector<Task*> tick() {
        current_time++;
        
        // Cascade from highest to lowest
        if (current_time % 3600 == 0) {
            cascade(hoursWheel);
        }
        if (current_time % 60 == 0) {
            cascade(minutesWheel);
        }

        std::vector<Task*> executed_tasks;
        size_t s_slot = current_time % 60;
        TaskNode* curr = secondsWheel->slots[s_slot];
        secondsWheel->slots[s_slot] = nullptr;

        while (curr) {
            TaskNode* next = curr->next;
            curr->wheel = nullptr;
            executed_tasks.push_back(curr->task);
            
            if (curr->task->getPeriod() > 0) {
                curr->trigger_time += curr->task->getPeriod();
                addNodeToAppropriateWheel(curr);
            } else {
                delete curr;
            }
            curr = next;
        }

        return executed_tasks;
    }

private:
    size_t current_time;
    TimingWheel* secondsWheel;
    TimingWheel* minutesWheel;
    TimingWheel* hoursWheel;

    void addNodeToAppropriateWheel(TaskNode* node) {
        size_t D = node->trigger_time - current_time;
        if (D < 60) {
            node->slot = node->trigger_time % 60;
            secondsWheel->addNode(node);
        } else if (D < 3600) {
            node->slot = (node->trigger_time / 60) % 60;
            minutesWheel->addNode(node);
        } else {
            node->slot = (node->trigger_time / 3600) % 24;
            hoursWheel->addNode(node);
        }
    }

    void cascade(TimingWheel* wheel) {
        size_t slot = (current_time / wheel->interval) % wheel->size;
        TaskNode* curr = wheel->slots[slot];
        wheel->slots[slot] = nullptr;
        
        while (curr) {
            TaskNode* next = curr->next;
            curr->wheel = nullptr;
            addNodeToAppropriateWheel(curr);
            curr = next;
        }
    }
};
