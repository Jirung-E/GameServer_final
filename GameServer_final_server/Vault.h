#pragma once

#include <mutex>


/// <summary>
/// Mutex wrapper
/// </summary>
/// <typeparam name="T"></typeparam>
template<class T>
class Vault {
private:
    T data;
    std::mutex mtx;

public:
    class Borrowed {
        friend class Vault;

    private:
        T& data;
        std::unique_lock<std::mutex> guard;

    private:
        Borrowed(T& data, std::mutex& mtx): data { data }, guard { mtx } {

        }
        Borrowed(const Vault& other) = delete;
        Borrowed operator=(const Vault& other) = delete;

    public:
        T& operator*() {
            return data;
        }
        T* operator->() {
            return &data;
        }

    public:
        void release() {
            guard.unlock();
        }
    };

public:
    template<typename... Args>
    explicit Vault(Args&&... args): data { std::forward<Args>(args)... }
    {

    }

    Vault(const Vault&) = delete;
    Vault& operator=(const Vault&) = delete;
    Vault(Vault&&) = delete;
    Vault& operator=(Vault&&) = delete;

public:
    Borrowed borrow() {
        return Borrowed { data, mtx };
    }
};