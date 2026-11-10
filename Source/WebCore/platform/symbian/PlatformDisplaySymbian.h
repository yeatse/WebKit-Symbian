/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include "PlatformDisplay.h"

namespace WebCore {
class PlatformDisplaySymbian final : public PlatformDisplay {
public:
    static std::unique_ptr<PlatformDisplaySymbian> create();
    Type type() const final { return Type::Symbian; }
private:
    explicit PlatformDisplaySymbian(Ref<GLDisplay>&&);
};
}
