#include "mutex.h"
#include <stdexcept>

namespace zmuduo {
Semaphore::Semaphore(const unsigned int count)
    : m_semaphore() {
    if (sem_init(&m_semaphore, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
    if (sem_wait(&m_semaphore)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if (sem_post(&m_semaphore)) {
        throw std::logic_error("sem_post error");
    }
}
} // namespace zmuduo