/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <services/sms/common.h>

#include <type_traits>

static_assert(std::has_virtual_destructor_v<eka2l1::epoc::sms::sms_pdu>,
    "SMS PDUs are owned through sms_pdu pointers and must be destroyed polymorphically");
