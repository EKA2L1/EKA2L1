/*
 * Copyright (c) 2021 EKA2L1 Team
 * Copyright (c) 2020 Yury Kharchenko
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

package com.github.eka2l1.util;

import android.util.SparseIntArray;

import com.google.gson.TypeAdapter;
import com.google.gson.stream.JsonReader;
import com.google.gson.stream.JsonToken;
import com.google.gson.stream.JsonWriter;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;

public class SparseIntArrayAdapter extends TypeAdapter<SparseIntArray> {
	@Override
	public void write(JsonWriter out, SparseIntArray array) throws IOException {
		out.beginObject();
		for (int i = 0; i < array.size(); i++) {
			out.name(Integer.toString(array.keyAt(i))).value(array.valueAt(i));
		}
		out.endObject();
	}

	@Override
	public SparseIntArray read(JsonReader in) throws IOException {
		JsonToken peek = in.peek();
		if (peek == JsonToken.NULL) {
			in.nextNull();
			return null;
		}
		SparseIntArray array = new SparseIntArray();
		if (peek == JsonToken.STRING) {
			String s = in.nextString();
			try {
				JSONArray jsonArray = new JSONArray(s);
				for (int i = 0; i < jsonArray.length(); i++) {
					JSONObject item = jsonArray.getJSONObject(i);
					array.put(item.getInt("key"), item.getInt("value"));
				}
			} catch (JSONException e) {
				e.printStackTrace();
			}
			return array;
		}
		in.beginObject();
		while (in.hasNext()) {
			String name = in.nextName();
			int key = Integer.parseInt(name);
			int value = in.nextInt();
			array.put(key, value);
		}
		in.endObject();
		return array;
	}
}
