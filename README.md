# promise.hpp

> C++ asynchronous promises like a Promises/A+

[![linux][badge.linux]][linux]
[![darwin][badge.darwin]][darwin]
[![windows][badge.windows]][windows]
[![codecov][badge.codecov]][codecov]
[![language][badge.language]][language]
[![license][badge.license]][license]

[badge.darwin]: https://img.shields.io/github/workflow/status/BlackMATov/promise.hpp/darwin/main?label=Xcode&logo=xcode
[badge.linux]: https://img.shields.io/github/workflow/status/BlackMATov/promise.hpp/linux/main?label=GCC%2FClang&logo=linux
[badge.windows]: https://img.shields.io/github/workflow/status/BlackMATov/promise.hpp/windows/main?label=Visual%20Studio&logo=visual-studio
[badge.codecov]: https://img.shields.io/codecov/c/github/BlackMATov/promise.hpp/main?logo=codecov
[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow
[badge.license]: https://img.shields.io/badge/license-MIT-blue

[darwin]: https://github.com/BlackMATov/promise.hpp/actions?query=workflow%3Adarwin
[linux]: https://github.com/BlackMATov/promise.hpp/actions?query=workflow%3Alinux
[windows]: https://github.com/BlackMATov/promise.hpp/actions?query=workflow%3Awindows
[codecov]: https://codecov.io/gh/BlackMATov/promise.hpp
[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[license]: https://en.wikipedia.org/wiki/MIT_License

[promise]: https://github.com/BlackMATov/promise.hpp

## Requirements

- [gcc](https://www.gnu.org/software/gcc/) **>= 7**
- [clang](https://clang.llvm.org/) **>= 7**
- [msvc](https://visualstudio.microsoft.com/) **>= 2017**

## Installation

[promise.hpp][promise] is a header-only library. All you need to do is copy the headers files from `headers` directory into your project and include them:

```cpp
#include "promise.hpp/promise.hpp"
```

Also, you can add the root repository directory to your [cmake](https://cmake.org) project:

```cmake
add_subdirectory(external/promise.hpp)
target_link_libraries(your_project_target promise.hpp)
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
        [result](const std::string& html) mutable
        {
            result.resolve(html);
        },
        [result](int error_code) mutable
        {
            result.reject(std::runtime_error("error code: " + std::to_string(error_code)));
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
            [resolver](const std::string& html) mutable
            {
                resolver(html);
            },
            [rejector](int error_code) mutable
            {
                rejector(std::runtime_error("error code: " + std::to_string(error_code)));
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

make_all_promise(url_promises)
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
        std::vector<promise<std::string>> url_promises;
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

## [License (MIT)](./LICENSE.md)
