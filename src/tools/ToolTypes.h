#pragma once
#include <functional>
#include <QString>

namespace Tools {
    using LogCb = std::function<void(const QString &)>;
}
