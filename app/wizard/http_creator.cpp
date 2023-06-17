#include "stdafx.h"
#include "file_tmpl.h"
#include "http_creator.h"

//////////////////////////////////////////////////////////////////////////

static bool set_cookies(tpl_t* tpl)
{
	printf("Please enter cookie name: "); fflush(stdout);
	char line[256];
	int n = acl_vstream_gets_nonl(ACL_VSTREAM_IN, line, sizeof(line));
	if (n == ACL_VSTREAM_EOF) {
		printf("enter cookie name error!\r\n");
		return false;
	}

	string buf;
	buf.format("const char* %s = req.getCookieValue(\"%s\");\r\n", line, line);
	buf.format_append("\tif (%s == NULL) {\r\n\t\tres.addCookie(\"%s\", \"{xxx}\");\r\n\t}\r\n", line, line);

	tpl_set_field_fmt_global(tpl, "GET_COOKIES", "%s", buf.c_str());
	return true;
}

//////////////////////////////////////////////////////////////////////////

static bool create_threads(file_tmpl& tmpl)
{
	string file(tmpl.get_project_name());
	file << ".cf";
	if (tmpl.copy_and_replace("master_threads.cf", file.c_str()) == false) {
		return false;
	}

	const char* name = "master_threads";
	const FILE_FROM_TO tab[] = {
		{ "main_threads.cpp",	"main.cpp"		},
		{ "master_threads.h",	"master_service.h"	},
		{ "master_threads.cpp",	"master_service.cpp"	},
		{ "http_service.h",	"http_service.h"	},
		{ "http_service.cpp",	"http_service.cpp"	},
		{ "http_servlet.h",	"http_servlet.h"	},
		{ "websocket.cpp",	"websocket.cpp"		},
		{ "websocket.h",	"websocket.h"		},

		{ NULL, NULL }
	};

	return tmpl.files_copy(name, tab);
}

static bool create_fiber(file_tmpl& tmpl)
{
	string file(tmpl.get_project_name());
	file << ".cf";
	if (tmpl.copy_and_replace("master_fiber.cf", file.c_str()) == false)
		return false;

	const char* name = "master_fiber";
	const FILE_FROM_TO tab[] = {
		{ "stdafx_fiber.h",	"stdafx.h"		},
		{ "main_fiber.cpp",	"main.cpp"		},
		{ "master_fiber.h",	"master_service.h"	},
		{ "master_fiber.cpp",	"master_service.cpp"	},
		{ "http_service.h",	"http_service.h"	},
		{ "http_service.cpp",	"http_service.cpp"	},
		{ "http_servlet.h",	"http_servlet.h"	},
		{ "websocket.cpp",	"websocket.cpp"		},
		{ "websocket.h",	"websocket.h"		},

		{ NULL, NULL }
	};

	return tmpl.files_copy(name, tab)
		&& tmpl.copy_and_replace("Makefile_fiber", "Makefile")
		&& tmpl.file_copy("tmpl/Makefile_fiber.in",
			"Makefile.in");
}

static bool create_proc(file_tmpl& tmpl)
{
	string file(tmpl.get_project_name());
	file << ".cf";
	if (tmpl.copy_and_replace("master_proc.cf", file.c_str()) == false) {
		return false;
	}

	const char* name = "master_proc";
	const FILE_FROM_TO tab[] = {
		{ "main_proc.cpp",	"main.cpp"		},
		{ "master_proc.h",	"master_service.h"	},
		{ "master_proc.cpp",	"master_service.cpp"	},
		{ "http_service.h",	"http_service.h"	},
		{ "http_service.cpp",	"http_service.cpp"	},
		{ "http_servlet.h",	"http_servlet.h"	},
		{ "websocket.cpp",	"websocket.cpp"		},
		{ "websocket.h",	"websocket.h"		},

		{ NULL, NULL }
	};

	return tmpl.files_copy(name, tab);
}

static bool create_service(file_tmpl& tmpl, const char* type)
{
	char  buf[256];
	if (type == NULL || *type == 0) {
		printf("choose master_service type:\r\n");
		printf("	t: for master_threads\r\n"
				"	f: for master_fiber\r\n"
				"	p: for master_proc\r\n");
		printf(">");
		fflush(stdout);

		int   n = acl_vstream_gets_nonl(ACL_VSTREAM_IN, buf, sizeof(buf));
		if (n == ACL_VSTREAM_EOF) {
			return false;
		}

		type = buf;
	}

	if (strcasecmp(type, "t") == 0) {
		create_threads(tmpl);
	} else if (strcasecmp(type, "p") == 0) {
		create_proc(tmpl);
	} else if (strcasecmp(type, "f") == 0) {
		create_fiber(tmpl);
	} else {
		printf("invalid type: %s\r\n", type);
		return false;
	}

	return true;
}

static bool create_http_servlet(file_tmpl& tmpl, const char* type)
{
	tpl_t* tpl = tmpl.open_tpl("http_servlet.cpp");
	if (tpl == NULL) {
		return false;
	}

	printf("Do you want add cookie? [y/n]: ");
	fflush(stdout);

	char buf[64];
	int n = acl_vstream_gets_nonl(ACL_VSTREAM_IN, buf, sizeof(buf));
	if (n != ACL_VSTREAM_EOF && strcasecmp(buf, "y") == 0) {
		set_cookies(tpl);
	}

	string filepath;
	filepath.format("%s/http_servlet.cpp", tmpl.get_project_name());
	if (tpl_save_as(tpl, filepath.c_str()) != TPL_OK) {
		printf("save to %s error: %s\r\n", filepath.c_str(),
			last_serror());
		tpl_free(tpl);
		return false;
	}

	printf("create %s ok.\r\n", filepath.c_str());
	tpl_free(tpl);

	// 设置服务器模板类型
	return create_service(tmpl, type);
}

void http_creator(const char* name, const char* type)
{
	bool loop;
	file_tmpl tmpl;

	if (name && *name && type && *type) {
		loop = true;
	} else {
		loop = false;
	}

	// 设置源程序所在目录
	tmpl.set_path_from("tmpl/http");

	while (true) {
		char buf[256];
		int  n;

		if (name == NULL || *name == 0) {
			printf("please input your program name: ");
			fflush(stdout);
			n = acl_vstream_gets_nonl(ACL_VSTREAM_IN, buf, sizeof(buf));
			if (n == ACL_VSTREAM_EOF) {
				break;
			}

			if (n == 0) {
				acl::safe_snprintf(buf, sizeof(buf), "http_demo");
			}

			name = buf;
		}

		tmpl.set_project_name(name);

		// 创建目录
		tmpl.create_dirs();

		tmpl.create_common();
		create_http_servlet(tmpl, type);
		break;
	}
}
