# Финальный проект: Поисковая система
SearchServer - система поиска документов по ключевым словам.

## Основные функции:
- Результаты поиска упорядочиваются в соответствии с использованием статистической меры TF-IDF (Частота Термина - Обратная Частота Документа), которая определяет значимость и редкость терминов, используемых в запросе, по отношению к общему корпусу документов;
- Стоп-слова исключаются из обработки (не учитываются при поиске и не влияют на результаты);
- Документы, содержащие минус-слова, исключаются из результатов поиска;
- Формирование и обработка очереди запросов;
- Удаление повторяющихся документов из результатов поиска;
- Разделение результатов поиска на страницы для удобства просмотра и навигации;
- Поддержка многопоточности;
## Принцип работы
- Создание экземпляра класса SearchServer. В конструктор передаётся строка со стоп-словами, разделенными пробелами. Вместо строки можно передавать произвольный контейнер (с последовательным доступом к элементам с возможностью использования в for-range цикле)

- С помощью метода AddDocument добавляются документы для поиска. В метод передаётся id документа, статус, рейтинг, и сам документ в формате строки.

- Метод FindTopDocuments возвращает вектор документов, согласно соответствию переданным ключевым словам. Результаты отсортированы по статистической мере TF-IDF. Возможна дополнительная фильтрация документов по id, статусу и рейтингу. Метод реализован как в однопоточной так и в многопоточной версии.

- Класс RequestQueue реализует очередь запросов к поисковому серверу с сохранением результатов поиска.

## Сборка и установка
Сборка с помощью любой IDE либо сборка из командной строки

## Системные требования
- C++17 или новее
