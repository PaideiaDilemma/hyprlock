#pragma once

#include "./Log.hpp"
#include <cstring>
#include <sodium.h>

class SensitiveString {
  public:
    static constexpr size_t FIXED_BUFFER_SIZE          = 4096;
    SensitiveString(const SensitiveString&)            = delete;
    SensitiveString& operator=(const SensitiveString&) = delete;

    SensitiveString() {
        RASSERT(sodium_init() >= 0, "sodium_init failed");
        m_pData    = (char*)sodium_malloc(FIXED_BUFFER_SIZE);
        m_iLength  = 0;
        m_pData[0] = '\0';
    }

    SensitiveString(const char* data) {
        RASSERT(sodium_init() >= 0, "sodium_init failed");
        m_pData = (char*)sodium_malloc(FIXED_BUFFER_SIZE);
        set(data);
    }

    ~SensitiveString() {
        clear();
        sodium_free(m_pData);
        m_pData = nullptr;
    }

    inline void set(const char* data) {
        const auto LEN = strlen(data);
        if (LEN >= FIXED_BUFFER_SIZE) {
            Debug::log(ERR, "SensitiveString: data too large");
            clear();
            return;
        }
        RASSERT(memcpy(m_pData, data, LEN + 1) == m_pData, "memcpy failed");
        m_iLength = LEN;
    }

    inline void set(const SensitiveString& other) {
        if (other.m_iLength >= FIXED_BUFFER_SIZE) {
            Debug::log(ERR, "SensitiveString: data too large");
            clear();
            return;
        }
        RASSERT(memcpy(m_pData, other.m_pData, other.m_iLength + 1) == m_pData, "memcpy failed");
        m_iLength = other.m_iLength;
    }

    char* c_str() {
        return m_pData;
    }

    size_t length() {
        return m_iLength;
    }

    void clear() {
        sodium_memzero(m_pData, FIXED_BUFFER_SIZE);
        m_iLength = 0;
    }

    char back() {
        if (m_iLength == 0)
            return '\0';
        return m_pData[m_iLength - 1];
    }

    char pop_back() {
        if (m_iLength == 0)
            return '\0';
        m_iLength--;
        const auto C       = m_pData[m_iLength];
        m_pData[m_iLength] = '\0';
        return C;
    }

    void extend(char* buf, size_t len) {
        if (m_iLength + len >= FIXED_BUFFER_SIZE) {
            Debug::log(ERR, "SensitiveString: data too large");
            clear();
            return;
        }
        memcpy(m_pData + m_iLength, buf, len);
        m_iLength += len;
        m_pData[m_iLength] = '\0';
    }

    bool empty() {
        return m_iLength == 0;
    }

    char* begin() const {
        return m_pData;
    }
    char* end() const {
        return m_pData + m_iLength;
    }

  private:
    char*  m_pData   = nullptr;
    size_t m_iLength = 0;
};
