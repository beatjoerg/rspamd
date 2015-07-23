/*
 * Copyright (c) 2015, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "lua_common.h"
#include "message.h"
#include "html.h"
#include "images.h"

/***
 * @module rspamd_html
 * This module provides different methods to access HTML tags. To get HTML context
 * from an HTML part you could use method `part:get_html()`
 * @example
rspamd_config.R_HTML_IMAGE = function (task)
	parts = task:get_text_parts()
	if parts then
		for _,part in ipairs(parts) do
			if part:is_html() then
				local html = part:get_html()
				-- Do something with html
			end
		end
	end
	return false
end
 */

/***
 * @method html:has_tag(name)
 * Checks if a specified tag `name` is presented in a part
 * @param {string} name name of tag to check
 * @return {boolean} `true` if the tag exists in HTML tree
 */
LUA_FUNCTION_DEF (html, has_tag);

/***
 * @method html:check_property(name)
 * Checks if the HTML has a specific property. Here is the list of available properties:
 *
 * - `no_html` - no html tag presented
 * - `bad_element` - part has some broken elements
 * - `xml` - part is xhtml
 * - `unknown_element` - part has some unknown elements
 * - `duplicate_element` - part has some duplicate elements that should be unique (namely, `title` tag)
 * - `unbalanced` - part has unbalanced tags
 * @param {string} name name of property
 * @return {boolean} true if the part has the specified property
 */
LUA_FUNCTION_DEF (html, has_property);

/***
 * @method html:get_images()
 * Returns table of images found in html. Each image is, in turn, a table with the following fields:
 *
 * - `src` - link to the source
 * - `height` - height in pixels
 * - `width` - width in pixels
 * - `embeded` - `true` if an image is embedded in a message
 * @return {table} table of images in html part
 */
LUA_FUNCTION_DEF (html, get_images);

static const struct luaL_reg htmllib_m[] = {
	LUA_INTERFACE_DEF (html, has_tag),
	LUA_INTERFACE_DEF (html, has_property),
	LUA_INTERFACE_DEF (html, get_images),
	{"__tostring", rspamd_lua_class_tostring},
	{NULL, NULL}
};

static struct html_content *
lua_check_html (lua_State * L, gint pos)
{
	void *ud = luaL_checkudata (L, pos, "rspamd{html}");
	luaL_argcheck (L, ud != NULL, pos, "'html' expected");
	return ud ? *((struct html_content **)ud) : NULL;
}

static gint
lua_html_has_tag (lua_State *L)
{
	struct html_content *hc = lua_check_html (L, 1);
	const gchar *tagname = luaL_checkstring (L, 2);
	gboolean ret = FALSE;

	if (hc && tagname) {
		if (rspamd_html_tag_seen (hc, tagname)) {
			ret = TRUE;
		}
	}

	lua_pushboolean (L, ret);

	return 1;
}

static gint
lua_html_has_property (lua_State *L)
{
	struct html_content *hc = lua_check_html (L, 1);
	const gchar *propname = luaL_checkstring (L, 2);
	gboolean ret = FALSE;

	if (hc && propname) {
		/*
		 * - `no_html`
		 * - `bad_element`
		 * - `xml`
		 * - `unknown_element`
		 * - `duplicate_element`
		 * - `unbalanced`
		 */
		if (strcmp (propname, "no_html") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_BAD_START;
		}
		else if (strcmp (propname, "bad_element") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_BAD_ELEMENTS;
		}
		else if (strcmp (propname, "xml") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_XML;
		}
		else if (strcmp (propname, "unknown_element") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_UNKNOWN_ELEMENTS;
		}
		else if (strcmp (propname, "duplicate_element") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_DUPLICATE_ELEMENTS;
		}
		else if (strcmp (propname, "unbalanced") == 0) {
			ret = hc->flags & RSPAMD_HTML_FLAG_UNBALANCED;
		}
	}

	lua_pushboolean (L, ret);

	return 1;
}

static gint
lua_html_get_images (lua_State *L)
{
	struct html_content *hc = lua_check_html (L, 1);
	struct html_image *img;
	guint i;

	if (hc != NULL) {
		lua_newtable (L);

		if (hc->images) {
			for (i = 0; i < hc->images->len; i ++) {
				img = g_ptr_array_index (hc->images, i);

				lua_newtable (L);

				if (img->src) {
					lua_pushstring (L, "src");
					lua_pushstring (L, img->src);
					lua_settable (L, -3);
				}

				lua_pushstring (L, "height");
				lua_pushnumber (L, img->height);
				lua_settable (L, -3);
				lua_pushstring (L, "width");
				lua_pushnumber (L, img->width);
				lua_settable (L, -3);
				lua_pushstring (L, "embedded");
				lua_pushboolean (L, img->flags & RSPAMD_HTML_FLAG_IMAGE_EMBEDDED);
				lua_settable (L, -3);

				lua_rawseti (L, -2, i + 1);
			}
		}
	}
	else {
		lua_pushnil (L);
	}

	return 1;
}

void
luaopen_html (lua_State * L)
{
	rspamd_lua_new_class (L, "rspamd{html}", htmllib_m);
	lua_pop (L, 1);
}
