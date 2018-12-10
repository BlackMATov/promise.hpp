# promise.hpp

[![travis][badge.travis]][travis]
[![appveyor][badge.appveyor]][appveyor]
[![language][badge.language]][language]
[![license][badge.license]][license]
[![paypal][badge.paypal]][paypal]

[badge.travis]: https://img.shields.io/travis/BlackMATov/promise.hpp/master.svg?logo=travis&style=for-the-badge
[badge.appveyor]: https://img.shields.io/appveyor/ci/BlackMATov/promise-hpp/master.svg?logo=appveyor&style=for-the-badge
[badge.language]: https://img.shields.io/badge/language-C%2B%2B14-red.svg?style=for-the-badge
[badge.license]: https://img.shields.io/badge/license-MIT-blue.svg?style=for-the-badge
[badge.paypal]: https://img.shields.io/badge/donate-PayPal-orange.svg?logo=paypal&colorA=00457C&style=for-the-badge

[travis]: https://travis-ci.org/BlackMATov/promise.hpp
[appveyor]: https://ci.appveyor.com/project/BlackMATov/promise-hpp
[language]: https://en.wikipedia.org/wiki/C%2B%2B14
[license]: https://en.wikipedia.org/wiki/MIT_License
[paypal]: https://www.paypal.me/matov

[promise]: https://github.com/BlackMATov/promise.hpp

## Installation

[promise.hpp][promise] is a single header library. All you need to do is copy the header file into your project and include this file:

```cpp
#include "promise.hpp"
```

## Examples

### Creating a promise

```cpp
#include "promise.hpp"
using promise_hpp;

promise<std::string> download(const std::string& url)
{
    promise<std::string> result;

    web_client.download_with_callbacks(
        [result](const std::string& html)
        {
            result.resolve(html);
        },
        [result](int error_code)
        {
            result.reject(std::runtime_error("error code: " + std::string(error_code)));
        });

    return result;
}
```

### Alternative way to create a promise

```cpp
promise<std::string> p = make_promise<std::string>(
    [](auto&& resolver, auto&& rejector)
    {
        web_client.download_with_callbacks(
            [resolver](const std::string& html)
            {
                resolver(html);
            },
            [rejector](int error_code)
            {
                rejector(std::runtime_error("error code: " + std::string(error_code)));
            });
    });
```

### Waiting for an async operation

```cpp
download("http://www.google.com")
    .then([](const std::string& html)
    {
        std::cout << html << std::endl;
    })
    .except([](std::exception_ptr e)
    {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ee) {
            std::cerr << ee.what() << std::endl;
        }
    });
```

### Chaining async operations

```cpp
download("http://www.google.com")
    .then([](const std::string& html)
    {
        return download(extract_first_link(html));
    })
    .then([](const std::string& first_link_html)
    {
        std::cout << first_link_html << std::endl;
    })
    .except([](std::exception_ptr e)
    {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ee) {
            std::cerr << ee.what() << std::endl;
        }
    });
```

### Transforming promise results

```cpp
download("http://www.google.com")
    .then([](const std::string& html)
    {
        return extract_all_links(html);
    })
    .then([](const std::vector<std::string>& links)
    {
        for ( const std::string& link : links ) {
            std::cout << link << std::endl;
        }
    });
```

### Combining multiple async operations

```cpp
std::vector<std::string> urls{
    "http://www.google.com",
    "http://www.yandex.ru"};

std::vector<promise<std::string>> url_promises;
std::transform(urls.begin(), urls.end(), url_promises.begin(), download);

pr::make_all_promise(url_promises)
    .then([](const std::vector<std::string>& pages)
    {
        std::vector<std::string> all_links;
        for ( const std::string& page : pages ) {
            std::vector<std::string> page_links = extract_all_links(page);
            all_links.insert(
                all_links.end(),
                std::make_move_iterator(page_links.begin()),
                std::make_move_iterator(page_links.end()));
        }
        return all_links;
    })
    .then([](const std::vector<std::string>& all_links)
    {
        for ( const std::string& link : all_links ) {
            std::cout << link << std::endl;
        }
    });
```

### Chaining multiple async operations

```cpp
download("http://www.google.com")
    .then_all([](const std::string& html)
    {
        std::vector<pr::promise<std::string>> url_promises;
        std::vector<std::string> links = extract_all_links(html);
        std::transform(links.begin(), links.end(), url_promises.begin(), download);
        return url_promises;
    })
    .then([](const std::vector<std::string>& all_link_page_htmls)
    {
        // ...
    })
    .except([](std::exception_ptr e)
    {
        // ...
    });
```

## API

> coming soon!

## [License (MIT)](./LICENSE.md)
