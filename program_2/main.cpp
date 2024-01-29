#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <sstream>
#include <unistd.h>
#include <vector>

// Объявление глобальных переменных
int buyerSleep;                                     // Задержка потока покупателя
int salesmanSleep;                                  // Задержка потока продавца
std::vector<std::string> answer(0);              // Массив сообщений который будет записан в файл
std::vector<int> buffer(3);                      // Массив передающий продавцам номер покупателя
std::vector<pthread_cond_t> needed(3);           // Массив условных перменных сообщающих о вызове продавца
std::vector<pthread_mutex_t> needed_mutex(3);    // Мютексы для продавцов
std::vector<pthread_cond_t> available(3);        // Массив условных перменных сообщающих что продавец свободен
std::vector<pthread_mutex_t> available_mutex(3); // Мютексы для покупателей
std::vector<bool> sellerBusy(3, false);    // Свободен ли продавец
pthread_mutex_t mutex;                              // Мютекс записи сообщений в общий массив

/// Функция чтения данных из файла
/// \param inputFile - Путь к читаемому файлу
/// \param buyersCount - Переменная для записи количества покупателей
/// \param outputFile - Переменная для записи пути к файлу в который будут записаны сообщения
/// \return - Удачно ли прочитан файл
bool readConfig(const std::string &inputFile, int &buyersCount, std::string &outputFile);

/// Функция записи сообщений в файл
/// \param outputFile - Путь к файлу
/// \return - Успешно ли проведена запись
bool writeConfig(const std::string &outputFile);

/// Функция возвращающая случаное число в диапазоне [min; max]
/// \param min - Минимальное значение
/// \param max - Максимальное значение
/// \return - Случаное значение
int randomInteger(int min, int max);

/// Функция имитирующая продавца
/// \param param - Номер продавца
[[noreturn]] void *Salesman(void *param) {
    int num = *((int *) param); // Иницализация номера потока
    // Имитация работы Продавца
    while (true) {
        // Ожидание когда клиент обратиться к продавцу
        while (!sellerBusy[num]) {
            pthread_cond_wait(&needed[num], &needed_mutex[num]) ;
        }

        pthread_mutex_lock(&mutex); // Ожидание входа в критическую секцию mutex -1
        // Формирование сообщения
        std::ostringstream oss;
        oss << num << " the seller served " << buffer[num] << " the buyer";

        answer.push_back(oss.str()); // Запись сообщения в общий массив
        printf("%s\n", answer.back().c_str());  // Вывод сообщения в консоль
        pthread_mutex_unlock(&mutex); // Конец критической секции mutex +1
        sellerBusy[num] = false;
        pthread_cond_broadcast(&available[num]); // Продавец свободен
        sleep(salesmanSleep);
    }
}

/// Функция имитирующая покупателя
/// \param param - Номер покупателя
/// \return - Ничего
void *Buyer(void *param) {
    int num = *((int *) param); // Иницализация номера потока
    int n = randomInteger(1, 3); // Генерация количества отделов которые посетит покупатель
    // Имитация работы Покупателя
    for (int i = 0; i < n; ++i) {
        int k = randomInteger(0, 2); // Генерация номера отдела который посетит покупатель
        // Ожидание когда продавец освободиться
        while (sellerBusy[k]) {
            pthread_cond_wait(&available[k], &available_mutex[k]); // Ожидание когда осовободиться продавец
        }
        buffer[k] = num; // Запись в массив номера покупателя
        sellerBusy[k] = true;
        pthread_cond_broadcast(&needed[k]); // Вызов продавца
        sleep(buyerSleep);
    }
    return nullptr;
}


int main(int argc, char *argv[]) {
    // Создание переменных
    int buyersCount;            // Количество покупателей
    std::string outputFile;     // Путь к файлу в который будут записанны сообщения

    // Иницаиализация перменных
    if (argc != 2 && argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <number of customers>" << std::endl;
        return 1;
    } else if (argc == 2 && !readConfig(argv[1], buyersCount, outputFile)) {
        std::cerr << "Incorrect file path or content!!!" << std::endl;
        return 1;
    } else if (argc == 5) {
        try {
            buyersCount = std::stoi(argv[1]);
            buyerSleep = std::stoi(argv[2]);
            salesmanSleep = std::stoi(argv[3]);
            outputFile = argv[4];
        } catch (std::exception) {
            std::cerr << "Incorrect input!!" << std::endl;
            return 1;
        }
    }

    // Иницаиализация мютекса и семафоров
    pthread_mutex_init(&mutex, nullptr);    // Инициализация мютекса записи
    for (int i = 0; i < 3; ++i) {
        pthread_cond_init(&needed[i], NULL);
        pthread_cond_init(&available[i], NULL);
        pthread_mutex_init(&needed_mutex[i], nullptr);
        pthread_mutex_init(&available_mutex[i], nullptr);
    }

    // Создание потоков Продавцов
    pthread_t threadS[3]; // Адреса потоков
    int salesmans[3]; // Номера продавцов
    for (int i = 0; i < 3; ++i) {
        salesmans[i] = i; // Иницализация номера
        pthread_create(&threadS[i], nullptr, Salesman, (void *) (salesmans + i)); // Создание потока
    }

    // Создание потоков Покупателей
    pthread_t threadB[buyersCount]; // Адреса потоков
    int buyers[buyersCount]; // Номера покупателей
    for (int i = 0; i < buyersCount; ++i) {
        buyers[i] = i; // Иницализация номера
        pthread_create(&threadB[i], nullptr, Buyer, (void *) (buyers + i)); // Создание потока
        sleep(2);
    }

    // Ожидание и завершение потоков Покупателей
    for (int i = 0; i < buyersCount; ++i) {
        pthread_join(threadB[i], nullptr); // Ожидание i - го потока
    }

    // Завершение потоков Продавцов
    for (auto & i :threadB) {
        close(i); // Заверешение потока i
    }

    // Запись сообщений в файл и вывод сообщения при ошибке
    if (!writeConfig(outputFile)) {
        std::cerr << "Error writing data, incorrect path!!!" << std::endl;
    }

    return 0;
}

/// Функция возвращающая случаное число в диапазоне [min; max]
/// \param min - Минимальное значение
/// \param max - Максимальное значение
/// \return - Случаное значение
int randomInteger(int min, int max) {
    // Создает некриптографически безопасный генератор случайных чисел на основе аппаратного источника энтропии
    std::random_device rd;
    // Использует алгоритм mt19937 (Mersenne Twister) для генерации псевдослучайных чисел из seed, предоставленного random_device
    std::mt19937 gen(rd());
    // Определяет равномерное распределение целых чисел в заданном диапазоне [min, max]
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen); // Генерирует случайное целое число в заданном распределении и возвращает его
}


/// Функция чтения данных одной строки файла
/// \param inputFile - Путь к файлу
/// \return - Прочитанные данные
std::string readLine(std::ifstream &inputFile) {
    std::string str;
    getline(inputFile, str); // Чтение строки из файла
    str = str.substr(str.find('=') + 2); // Выделение из строки нужной информации
    return str;
}

/// Функция чтения данных из файла
/// \param inputFile - Путь к читаемому файлу
/// \param buyersCount - Переменная для записи количества покупателей
/// \param outputFile - Переменная для записи пути к файлу в который будут записаны сообщения
/// \return - Удачно ли прочитан файл
bool readConfig(const std::string &inputFile, int &buyersCount, std::string &outputFile) {
    try {
        std::ifstream file(inputFile); // Создание потока для чтения данных
        buyersCount = std::stoi(readLine(file)); // Запись количества покупателей
        buyerSleep = std::stoi(readLine(file));
        salesmanSleep = std::stoi(readLine(file));
        outputFile = readLine(file); // Запись пути к файлу для записи
        file.close(); // Закрытие потока
    } catch (std::exception) {
        return false; // Произошли ошикбки
    }
    return true; // Успешно (без ошибок)
}

/// Функция записи сообщений в файл
/// \param outputFile - Путь к файлу
/// \return - Успешно ли проведена запись
bool writeConfig(const std::string &outputFile) {
    try {
        std::ofstream file(outputFile); // Создание потока для записи
        // Запись сообщений в файл
        for (const auto &i: answer) {
            file << i << std::endl;
        }

        file.close(); // Закрытие потока
    } catch (std::exception) {
        return false; // Произошли ошикбки
    }
    return true; // Успешно (без ошибок)
}
