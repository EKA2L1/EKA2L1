/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

namespace eka2l1::common {
    class pixel_plotter;

    enum svg_render_error {
        svg_err_ok = 0,
        svg_err_invalid = -1,
        svg_err_not_found = -2
    };

    /**
     * \brief Render SVG to a bitmap, through a pixel plotter.
     * 
     * The SVG must follow the specification of SVG version 1.1. SVG version 2 is not supported.
     * 
     * \param plotter  The pixel plotter handler of a bitmap.
     * \param xml_data SVG data as in XML form. The function can also detects GZIP-SVG form.
     * \param diag     Pointer to diag string, which will be filled with error description.
     * 
     * \returns 0 for success. Else, seek the enum svg_render_error.
     */
    int svg_render(pixel_plotter *plotter, const char *xml_data, std::string *diag);
}
