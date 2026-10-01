#include "system_icon.hpp"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Icon lookup follows the freedesktop.org Icon Theme Specification:
// https://specifications.freedesktop.org/icon-theme-spec/latest/
// GTK's GtkIconTheme is deliberately not used: it requires a display, is not
// thread-safe, and loading GTK into an Electron process causes conflicts.

enum class DirectoryType
{
  Fixed,
  Scalable,
  Threshold
};

struct ThemeDirectory
{
  std::string path;
  DirectoryType type;
  int size;
  int scale;
  int minSize;
  int maxSize;
  int threshold;
};

struct Theme
{
  std::vector<std::string> roots;
  std::vector<ThemeDirectory> directories;
  std::vector<std::string> inherits;
};

int GetPixelSize(IconSize size)
{
  switch (size)
  {
    case IconSize::ExtraSmall:
      return 16;
    case IconSize::Small:
      return 32;
    case IconSize::Medium:
      return 64;
    case IconSize::Large:
      return 256;
    case IconSize::ExtraLarge:
      return 512;
  }
  return 64;
}

std::string BuildPath(const std::string& a, const std::string& b)
{
  g_autofree gchar* path = g_build_filename(a.c_str(), b.c_str(), nullptr);
  return path;
}

std::string GetBasename(const std::string& path)
{
  g_autofree gchar* base = g_path_get_basename(path.c_str());
  return base;
}

std::vector<std::string> GetIconBaseDirectories()
{
  std::vector<std::string> dirs;
  dirs.push_back(BuildPath(g_get_home_dir(), ".icons"));
  dirs.push_back(BuildPath(g_get_user_data_dir(), "icons"));
  for (auto dir = g_get_system_data_dirs(); *dir != nullptr; ++dir)
  {
    dirs.push_back(BuildPath(*dir, "icons"));
  }
  return dirs;
}

std::string ReadKeyFileString(const std::string& path, const char* group,
                              const char* key)
{
  g_autoptr(GKeyFile) keyFile = g_key_file_new();
  if (!g_key_file_load_from_file(keyFile, path.c_str(), G_KEY_FILE_NONE,
                                 nullptr))
  {
    return std::string{};
  }

  g_autofree gchar* value =
    g_key_file_get_string(keyFile, group, key, nullptr);
  return value != nullptr ? value : std::string{};
}

std::string GetGSettingsThemeName()
{
  auto source = g_settings_schema_source_get_default();
  if (source == nullptr)
  {
    return std::string{};
  }

  // g_settings_new() aborts the process if the schema is not installed.
  g_autoptr(GSettingsSchema) schema =
    g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE);
  if (schema == nullptr || !g_settings_schema_has_key(schema, "icon-theme"))
  {
    return std::string{};
  }

  g_autoptr(GSettings) settings = g_settings_new_full(schema, nullptr, nullptr);
  g_autofree gchar* name = g_settings_get_string(settings, "icon-theme");
  return name != nullptr ? name : std::string{};
}

std::string GetThemeName()
{
  const std::string configDir = g_get_user_config_dir();
  const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");

  if (desktop != nullptr && std::strstr(desktop, "KDE") != nullptr)
  {
    auto name =
      ReadKeyFileString(BuildPath(configDir, "kdeglobals"), "Icons", "Theme");
    return name.empty() ? "breeze" : name;
  }

  auto name = GetGSettingsThemeName();
  if (!name.empty())
  {
    return name;
  }

  for (const char* gtk : {"gtk-4.0", "gtk-3.0"})
  {
    name = ReadKeyFileString(
      BuildPath(BuildPath(configDir, gtk), "settings.ini"), "Settings",
      "gtk-icon-theme-name");
    if (!name.empty())
    {
      return name;
    }
  }

  return "hicolor";
}

int GetInteger(GKeyFile* keyFile, const char* group, const char* key,
               int defaultValue)
{
  g_autoptr(GError) error = nullptr;
  auto value = g_key_file_get_integer(keyFile, group, key, &error);
  return error == nullptr ? value : defaultValue;
}

std::vector<std::string> GetList(GKeyFile* keyFile, const char* group,
                                 const char* key)
{
  std::vector<std::string> result;
  g_auto(GStrv) values =
    g_key_file_get_string_list(keyFile, group, key, nullptr, nullptr);
  if (values != nullptr)
  {
    for (auto value = values; *value != nullptr; ++value)
    {
      if (**value != '\0')
      {
        result.emplace_back(*value);
      }
    }
  }
  return result;
}

std::shared_ptr<const Theme> LoadTheme(const std::string& name)
{
  auto theme = std::make_shared<Theme>();
  g_autoptr(GKeyFile) index = nullptr;

  for (const auto& base : GetIconBaseDirectories())
  {
    auto root = BuildPath(base, name);
    if (!g_file_test(root.c_str(), G_FILE_TEST_IS_DIR))
    {
      continue;
    }

    theme->roots.push_back(root);

    if (index == nullptr)
    {
      g_autoptr(GKeyFile) keyFile = g_key_file_new();
      if (g_key_file_load_from_file(keyFile,
                                    BuildPath(root, "index.theme").c_str(),
                                    G_KEY_FILE_NONE, nullptr))
      {
        index = static_cast<GKeyFile*>(g_steal_pointer(&keyFile));
      }
    }
  }

  if (index == nullptr)
  {
    return nullptr;
  }

  // Lists in index.theme are comma-separated.
  g_key_file_set_list_separator(index, ',');

  auto paths = GetList(index, "Icon Theme", "Directories");
  for (const auto& path : GetList(index, "Icon Theme", "ScaledDirectories"))
  {
    paths.push_back(path);
  }

  std::set<std::string> seen;
  for (const auto& path : paths)
  {
    auto group = path.c_str();
    if (!seen.insert(path).second || !g_key_file_has_group(index, group))
    {
      continue;
    }

    ThemeDirectory dir;
    dir.path = path;
    dir.size = GetInteger(index, group, "Size", 0);
    if (dir.size <= 0)
    {
      continue;
    }
    dir.scale = GetInteger(index, group, "Scale", 1);
    dir.minSize = GetInteger(index, group, "MinSize", dir.size);
    dir.maxSize = GetInteger(index, group, "MaxSize", dir.size);
    dir.threshold = GetInteger(index, group, "Threshold", 2);

    g_autofree gchar* type =
      g_key_file_get_string(index, group, "Type", nullptr);
    if (type != nullptr && std::strcmp(type, "Fixed") == 0)
    {
      dir.type = DirectoryType::Fixed;
    }
    else if (type != nullptr && std::strcmp(type, "Scalable") == 0)
    {
      dir.type = DirectoryType::Scalable;
    }
    else
    {
      dir.type = DirectoryType::Threshold;
    }

    theme->directories.push_back(dir);
  }

  theme->inherits = GetList(index, "Icon Theme", "Inherits");
  return theme;
}

std::shared_ptr<const Theme> GetTheme(const std::string& name)
{
  static std::mutex mutex;
  static std::map<std::string, std::shared_ptr<const Theme>> cache;

  std::lock_guard<std::mutex> lock{mutex};
  auto it = cache.find(name);
  if (it == cache.end())
  {
    it = cache.emplace(name, LoadTheme(name)).first;
  }
  return it->second;
}

bool DirectoryMatchesSize(const ThemeDirectory& dir, int size)
{
  if (dir.scale != 1)
  {
    return false;
  }

  switch (dir.type)
  {
    case DirectoryType::Fixed:
      return dir.size == size;
    case DirectoryType::Scalable:
      return dir.minSize <= size && size <= dir.maxSize;
    case DirectoryType::Threshold:
      return dir.size - dir.threshold <= size &&
             size <= dir.size + dir.threshold;
  }
  return false;
}

int DirectorySizeDistance(const ThemeDirectory& dir, int size)
{
  int min = dir.size;
  int max = dir.size;

  switch (dir.type)
  {
    case DirectoryType::Fixed:
      break;
    case DirectoryType::Scalable:
      min = dir.minSize;
      max = dir.maxSize;
      break;
    case DirectoryType::Threshold:
      min = dir.size - dir.threshold;
      max = dir.size + dir.threshold;
      break;
  }

  if (size < min * dir.scale)
  {
    return min * dir.scale - size;
  }
  if (size > max * dir.scale)
  {
    return size - max * dir.scale;
  }
  return 0;
}

void AppendFilesInDirectory(const Theme& theme, const ThemeDirectory& dir,
                            const std::string& iconName,
                            std::vector<std::string>& files)
{
  for (const auto& root : theme.roots)
  {
    for (const char* extension : {".png", ".svg", ".xpm"})
    {
      auto path = BuildPath(BuildPath(root, dir.path), iconName + extension);
      if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
      {
        files.push_back(path);
      }
    }
  }
}

// Returns every file for the icon in the theme, best size first. Later ones
// are fallbacks for when a better one can't be loaded, like an SVG without
// gdk-pixbuf's SVG loader.
std::vector<std::string> LookupIcon(const Theme& theme,
                                    const std::string& iconName, int size)
{
  struct Candidate
  {
    bool exact;
    int distance;
    int size;
    std::vector<std::string> files;
  };

  std::vector<Candidate> candidates;
  for (const auto& dir : theme.directories)
  {
    Candidate candidate{DirectoryMatchesSize(dir, size),
                        DirectorySizeDistance(dir, size), dir.size * dir.scale,
                        {}};
    AppendFilesInDirectory(theme, dir, iconName, candidate.files);
    if (!candidate.files.empty())
    {
      candidates.push_back(std::move(candidate));
    }
  }

  // Exact matches in directory order, then the closest size, preferring larger
  // icons on a tie since scaling down looks better than scaling up.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) {
                     if (a.exact != b.exact)
                     {
                       return a.exact;
                     }
                     if (a.exact)
                     {
                       return false;
                     }
                     if (a.distance != b.distance)
                     {
                       return a.distance < b.distance;
                     }
                     return a.size > b.size;
                   });

  std::vector<std::string> files;
  for (const auto& candidate : candidates)
  {
    files.insert(files.end(), candidate.files.begin(), candidate.files.end());
  }
  return files;
}

// Calls `accept` with each icon file found for `iconNames`, in order of
// preference, until it returns true.
bool FindIcon(const std::vector<std::string>& iconNames, int size,
              const std::function<bool(const std::string&)>& accept)
{
  // Depth-first walk of the theme and its parents. Adwaita and hicolor are
  // appended as fallbacks so that minimal themes still produce an icon.
  std::set<std::string> visited;
  std::vector<std::shared_ptr<const Theme>> chain;
  std::function<void(const std::string&)> walk =
    [&](const std::string& name) {
      if (!visited.insert(name).second)
      {
        return;
      }
      auto theme = GetTheme(name);
      if (theme == nullptr)
      {
        return;
      }
      chain.push_back(theme);
      for (const auto& parent : theme->inherits)
      {
        walk(parent);
      }
    };
  walk(GetThemeName());
  walk("Adwaita");
  walk("hicolor");

  for (const auto& theme : chain)
  {
    for (const auto& iconName : iconNames)
    {
      for (const auto& path : LookupIcon(*theme, iconName, size))
      {
        if (accept(path))
        {
          return true;
        }
      }
    }
  }

  for (const auto& iconName : iconNames)
  {
    for (const char* extension : {".png", ".svg", ".xpm"})
    {
      auto path = BuildPath("/usr/share/pixmaps", iconName + extension);
      if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR) && accept(path))
      {
        return true;
      }
    }
  }

  return false;
}

// Returns the name of the gdk-pixbuf format for the file's extension, or an
// empty string if no loader handles it.
std::string GetFormatForFile(const std::string& path)
{
  auto name = GetBasename(path);
  auto dot = name.rfind('.');
  if (dot == std::string::npos)
  {
    return std::string{};
  }
  auto extension = name.substr(dot + 1);

  std::string result;
  GSList* formats = gdk_pixbuf_get_formats();
  for (auto item = formats; item != nullptr && result.empty();
       item = item->next)
  {
    auto format = static_cast<GdkPixbufFormat*>(item->data);
    g_auto(GStrv) extensions = gdk_pixbuf_format_get_extensions(format);
    for (auto e = extensions; e != nullptr && *e != nullptr; ++e)
    {
      if (g_ascii_strcasecmp(*e, extension.c_str()) == 0)
      {
        g_autofree gchar* formatName = gdk_pixbuf_format_get_name(format);
        result = formatName;
        break;
      }
    }
  }
  g_slist_free(formats);
  return result;
}

// Scales the image to fit `size` x `size`, keeping its aspect ratio, as
// gdk_pixbuf_new_from_file_at_scale() does.
void OnSizePrepared(GdkPixbufLoader* loader, int width, int height,
                    gpointer data)
{
  auto size = *static_cast<int*>(data);
  if (width <= 0 || height <= 0)
  {
    return;
  }

  if (width > height)
  {
    height = std::max(1, static_cast<int>(0.5 + static_cast<double>(height) *
                                                  size / width));
    width = size;
  }
  else
  {
    width = std::max(1, static_cast<int>(0.5 + static_cast<double>(width) *
                                                 size / height));
    height = size;
  }
  gdk_pixbuf_loader_set_size(loader, width, height);
}

// Loads the image with the loader for its extension. gdk-pixbuf otherwise
// picks one from the file's content type, which needs the shared MIME
// database: without it, every file is application/octet-stream and no loader
// accepts it.
GdkPixbuf* LoadPixbuf(const std::string& path, int size)
{
  auto format = GetFormatForFile(path);
  g_autofree gchar* contents = nullptr;
  gsize length = 0;
  if (!format.empty() &&
      g_file_get_contents(path.c_str(), &contents, &length, nullptr))
  {
    g_autoptr(GdkPixbufLoader) loader =
      gdk_pixbuf_loader_new_with_type(format.c_str(), nullptr);
    if (loader != nullptr)
    {
      g_signal_connect(loader, "size-prepared", G_CALLBACK(OnSizePrepared),
                       &size);
      auto written = gdk_pixbuf_loader_write(
        loader, static_cast<const guchar*>(static_cast<void*>(contents)),
        length, nullptr);
      // Always close the loader, even after a failed write.
      auto closed = gdk_pixbuf_loader_close(loader, nullptr);
      auto pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
      if (written && closed && pixbuf != nullptr)
      {
        return static_cast<GdkPixbuf*>(g_object_ref(pixbuf));
      }
    }
  }

  // The extension is unknown or wrong for the file's contents.
  return gdk_pixbuf_new_from_file_at_scale(path.c_str(), size, size, TRUE,
                                           nullptr);
}

std::vector<unsigned char> RenderFile(const std::string& path, int size)
{
  g_autoptr(GdkPixbuf) pixbuf = LoadPixbuf(path, size);
  if (pixbuf == nullptr)
  {
    return std::vector<unsigned char>{};
  }

  // Center non-square icons on a transparent canvas of the requested size.
  auto width = gdk_pixbuf_get_width(pixbuf);
  auto height = gdk_pixbuf_get_height(pixbuf);
  if (width != size || height != size)
  {
    g_autoptr(GdkPixbuf) source = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    GdkPixbuf* canvas =
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, size, size);
    gdk_pixbuf_fill(canvas, 0);
    gdk_pixbuf_copy_area(source, 0, 0, width, height, canvas,
                         (size - width) / 2, (size - height) / 2);
    g_object_unref(pixbuf);
    pixbuf = canvas;
  }

  gchar* buffer = nullptr;
  gsize length = 0;
  if (!gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &length, "png", nullptr,
                                 static_cast<char*>(nullptr)))
  {
    return std::vector<unsigned char>{};
  }

  auto p = static_cast<const unsigned char*>(static_cast<void*>(buffer));
  std::vector<unsigned char> result{p, p + length};
  g_free(buffer);
  return result;
}

std::vector<unsigned char> RenderIcon(GIcon* icon, int size)
{
  if (icon == nullptr)
  {
    return std::vector<unsigned char>{};
  }

  if (G_IS_EMBLEMED_ICON(icon))
  {
    return RenderIcon(g_emblemed_icon_get_icon(G_EMBLEMED_ICON(icon)), size);
  }

  if (G_IS_FILE_ICON(icon))
  {
    g_autofree gchar* path =
      g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon)));
    return path != nullptr ? RenderFile(path, size)
                           : std::vector<unsigned char>{};
  }

  std::vector<unsigned char> result;

  if (G_IS_THEMED_ICON(icon))
  {
    std::vector<std::string> names;
    for (auto name = g_themed_icon_get_names(G_THEMED_ICON(icon));
         *name != nullptr; ++name)
    {
      names.emplace_back(*name);
    }

    FindIcon(names, size, [&](const std::string& path) {
      result = RenderFile(path, size);
      return !result.empty();
    });
  }

  return result;
}

GIcon* GetAppIcon(GAppInfo* appInfo)
{
  if (appInfo == nullptr)
  {
    return nullptr;
  }

  auto icon = g_app_info_get_icon(appInfo);
  return icon != nullptr ? static_cast<GIcon*>(g_object_ref(icon)) : nullptr;
}

GIcon* GetIconForDesktopFile(const std::string& path)
{
  g_autoptr(GDesktopAppInfo) appInfo =
    g_desktop_app_info_new_from_filename(path.c_str());
  return GetAppIcon(G_APP_INFO(appInfo));
}

// Looks up a desktop file ID in the XDG data dirs, then in `extraDirs`, which
// may not be in $XDG_DATA_DIRS when Node is not started from a desktop session.
GIcon* GetIconForDesktopId(const std::string& id,
                           const std::vector<std::string>& extraDirs)
{
  g_autoptr(GDesktopAppInfo) appInfo = g_desktop_app_info_new(id.c_str());
  if (auto icon = GetAppIcon(G_APP_INFO(appInfo)))
  {
    return icon;
  }

  for (const auto& dir : extraDirs)
  {
    if (auto icon = GetIconForDesktopFile(BuildPath(dir, id)))
    {
      return icon;
    }
  }
  return nullptr;
}

GIcon* GetIconForPath(const std::string& path)
{
  // For desktop entries, use the application's own icon.
  if (g_str_has_suffix(path.c_str(), ".desktop"))
  {
    if (auto icon = GetIconForDesktopFile(path))
    {
      return icon;
    }
  }

  g_autoptr(GFile) file = g_file_new_for_path(path.c_str());
  g_autoptr(GFileInfo) info =
    g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON,
                      G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
  if (info == nullptr)
  {
    return nullptr;
  }

  auto icon = g_file_info_get_icon(info);
  return icon != nullptr ? static_cast<GIcon*>(g_object_ref(icon)) : nullptr;
}

GIcon* GetIconForExtension(const std::string& extension)
{
  std::string filename = "file";
  if (extension.empty() || extension[0] != '.')
  {
    filename += '.';
  }
  filename += extension;

  g_autofree gchar* type =
    g_content_type_guess(filename.c_str(), nullptr, 0, nullptr);
  return type != nullptr ? g_content_type_get_icon(type) : nullptr;
}

std::string GetProcPath(int pid, const char* entry)
{
  return "/proc/" + std::to_string(pid) + "/" + entry;
}

std::vector<std::string> ReadNulSeparated(const std::string& path)
{
  std::vector<std::string> result;
  gchar* contents = nullptr;
  gsize length = 0;
  if (!g_file_get_contents(path.c_str(), &contents, &length, nullptr))
  {
    return result;
  }

  for (gsize start = 0; start < length;)
  {
    auto end = start;
    while (end < length && contents[end] != '\0')
    {
      ++end;
    }
    result.emplace_back(contents + start, end - start);
    start = end + 1;
  }

  g_free(contents);
  return result;
}

std::string GetEnv(const std::vector<std::string>& environment,
                   const std::string& key)
{
  auto prefix = key + "=";
  for (const auto& entry : environment)
  {
    if (entry.compare(0, prefix.size(), prefix) == 0)
    {
      return entry.substr(prefix.size());
    }
  }
  return std::string{};
}

std::string ReadLink(const std::string& path)
{
  g_autofree gchar* target = g_file_read_link(path.c_str(), nullptr);
  if (target == nullptr)
  {
    return std::string{};
  }

  // The kernel marks executables that were replaced after the process started,
  // which is common after a package upgrade.
  std::string result = target;
  const std::string deleted = " (deleted)";
  if (g_str_has_suffix(result.c_str(), deleted.c_str()))
  {
    result.resize(result.size() - deleted.size());
  }
  return result;
}

std::string RealPath(const std::string& path)
{
  if (path.empty())
  {
    return path;
  }

  char* real = realpath(path.c_str(), nullptr);
  if (real == nullptr)
  {
    return std::string{};
  }

  std::string result = real;
  std::free(real);
  return result;
}

std::string ResolveProgram(const std::string& program)
{
  if (g_path_is_absolute(program.c_str()))
  {
    return program;
  }

  g_autofree gchar* path = g_find_program_in_path(program.c_str());
  return path != nullptr ? path : std::string{};
}

// Shells, interpreters and launchers don't identify an application by
// themselves; the script they run does.
bool IsGenericProgram(const std::string& path)
{
  static const std::set<std::string> generic{
    "bash", "dash",   "electron", "env", "flatpak", "java", "ksh",
    "mono", "node",   "perl",     "python", "ruby", "sh",  "snap",
    "wine", "zsh",
  };

  // Ignore version suffixes, as in python3.12.
  auto name = GetBasename(path);
  name.resize(name.find_last_not_of("0123456789.") + 1);
  return generic.count(name) > 0;
}

// Returns the first non-option argument if it names an existing file: for an
// interpreter, that is the script it runs.
std::string FindFileArgument(const std::vector<std::string>& args,
                             const std::string& cwd)
{
  for (std::size_t i = 1; i < args.size(); ++i)
  {
    const auto& arg = args[i];
    if (arg.empty() || arg[0] == '-')
    {
      continue;
    }

    std::string path;
    if (g_path_is_absolute(arg.c_str()))
    {
      path = arg;
    }
    else if (!cwd.empty())
    {
      path = BuildPath(cwd, arg);
    }

    if (!path.empty() && g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
    {
      return path;
    }
    return std::string{};
  }
  return std::string{};
}

// Returns the file that identifies the application a desktop entry launches,
// or an empty string if it can't be determined.
std::string GetDesktopProgram(GAppInfo* appInfo)
{
  auto commandline = g_app_info_get_commandline(appInfo);
  g_auto(GStrv) argv = nullptr;
  if (commandline == nullptr ||
      !g_shell_parse_argv(commandline, nullptr, &argv, nullptr))
  {
    return std::string{};
  }

  std::vector<std::string> args{argv, argv + g_strv_length(argv)};

  // Skip "env VAR=value ...".
  if (!args.empty() && GetBasename(args[0]) == "env")
  {
    std::size_t i = 1;
    while (i < args.size() &&
           (args[i].find('=') != std::string::npos || args[i][0] == '-'))
    {
      ++i;
    }
    args.erase(args.begin(), args.begin() + i);
  }

  if (args.empty())
  {
    return std::string{};
  }

  auto program = ResolveProgram(args[0]);
  if (program.empty())
  {
    return std::string{};
  }

  return RealPath(IsGenericProgram(program) ? FindFileArgument(args, "")
                                            : program);
}

// Finds the executable a process was started from when /proc/<pid>/exe can't
// be read, from argv[0] and the kernel's name for it (/proc/<pid>/comm).
// argv[0] isn't always the program: login shells prefix it with "-", systemd
// with "@", and some programs rewrite it to show their state, like
// "sshd: user [priv]", "postgres: checkpointer" or "redis-server *:6379".
std::string FindProgram(int pid, const std::vector<std::string>& args)
{
  std::vector<std::string> names;
  if (!args.empty() && !args[0].empty())
  {
    auto name = args[0];
    if (name[0] == '-' || name[0] == '@')
    {
      name.erase(0, 1);
    }
    names.push_back(name);
    names.push_back(name.substr(0, name.find(' ')));
    names.push_back(name.substr(0, name.find(':')));
  }

  g_autofree gchar* comm = nullptr;
  if (g_file_get_contents(GetProcPath(pid, "comm").c_str(), &comm, nullptr,
                          nullptr))
  {
    names.emplace_back(g_strchomp(comm));
  }

  for (const auto& name : names)
  {
    if (name.empty())
    {
      continue;
    }
    auto path = ResolveProgram(name);
    if (!path.empty() && g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
    {
      return path;
    }
  }

  // An absolute path may be in a directory we can't read, like another
  // user's home.
  if (!names.empty() && g_path_is_absolute(names[0].c_str()))
  {
    return names[0];
  }
  return std::string{};
}

// Returns the files that identify the process, most specific first.
std::vector<std::string> GetProcessExecutables(int pid)
{
  std::vector<std::string> result;
  auto args = ReadNulSeparated(GetProcPath(pid, "cmdline"));

  // /proc/<pid>/exe is only readable for our own processes. Kernel threads
  // have neither it nor a command line.
  auto exe = ReadLink(GetProcPath(pid, "exe"));
  if (exe.empty() && !args.empty())
  {
    exe = FindProgram(pid, args);
  }
  if (exe.empty())
  {
    return result;
  }

  if (IsGenericProgram(exe))
  {
    auto script = FindFileArgument(args, ReadLink(GetProcPath(pid, "cwd")));
    if (!script.empty())
    {
      result.push_back(script);
    }
  }

  result.push_back(exe);
  return result;
}

// Whether the file at `path` is a script that runs a program called `name` by
// its path, like "$HERE/chrome". Only the start of it is read, as a wrapper
// is short and a binary may not be.
bool ScriptRuns(const std::string& path, const std::string& name)
{
  FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr)
  {
    return false;
  }
  std::string script(64 * 1024, '\0');
  script.resize(std::fread(&script[0], 1, script.size(), file));
  std::fclose(file);

  if (script.compare(0, 2, "#!") != 0)
  {
    return false;
  }

  // Not as part of a longer name, like "/chrome_crashpad_handler".
  auto needle = "/" + name;
  for (auto i = script.find(needle); i != std::string::npos;
       i = script.find(needle, i + 1))
  {
    auto end = i + needle.size();
    if (end == script.size() ||
        !(g_ascii_isalnum(script[end]) || std::strchr("_.-", script[end])))
    {
      return true;
    }
  }
  return false;
}

GIcon* FindDesktopIconForExecutables(const std::vector<std::string>& executables)
{
  struct Entry
  {
    GAppInfo* appInfo;
    std::string program;
  };

  g_autolist(GAppInfo) apps = g_app_info_get_all();
  std::vector<Entry> entries;
  for (auto item = apps; item != nullptr; item = item->next)
  {
    auto appInfo = G_APP_INFO(item->data);
    if (g_app_info_get_icon(appInfo) == nullptr)
    {
      continue;
    }

    auto program = GetDesktopProgram(appInfo);
    if (!program.empty())
    {
      entries.push_back(Entry{appInfo, program});
    }
  }

  std::vector<std::string> candidates;
  for (const auto& executable : executables)
  {
    if (!IsGenericProgram(executable))
    {
      candidates.push_back(executable);
    }
  }

  // Exact match on the resolved executable.
  for (const auto& candidate : candidates)
  {
    auto real = RealPath(candidate);
    for (const auto& entry : entries)
    {
      if (entry.program == real)
      {
        return GetAppIcon(entry.appInfo);
      }
    }
  }

  // A desktop entry may launch a wrapper script that runs the application's
  // binary from beside it, as Chrome's runs "$HERE/chrome".
  for (const auto& candidate : candidates)
  {
    auto real = RealPath(candidate);
    if (real.empty())
    {
      continue;
    }
    g_autofree gchar* dir = g_path_get_dirname(real.c_str());
    auto name = GetBasename(real);
    for (const auto& entry : entries)
    {
      g_autofree gchar* entryDir = g_path_get_dirname(entry.program.c_str());
      if (std::strcmp(dir, entryDir) == 0 && entry.program != real &&
          ScriptRuns(entry.program, name))
      {
        return GetAppIcon(entry.appInfo);
      }
    }
  }

  // Match on the executable's name, then on the desktop file ID.
  for (const auto& candidate : candidates)
  {
    auto name = GetBasename(candidate);
    auto realName = GetBasename(RealPath(candidate));
    for (const auto& entry : entries)
    {
      auto entryName = GetBasename(entry.program);
      if (entryName == name || entryName == realName)
      {
        return GetAppIcon(entry.appInfo);
      }
    }

    if (auto icon = GetIconForDesktopId(name + ".desktop", {}))
    {
      return icon;
    }
  }

  return nullptr;
}

GIcon* GetIconForSnap(const std::string& snap)
{
  // Snap desktop files are named <snap>_<app>.desktop.
  const std::string dir = "/var/lib/snapd/desktop/applications";
  if (auto icon = GetIconForDesktopId(snap + "_" + snap + ".desktop", {dir}))
  {
    return icon;
  }

  std::vector<std::string> ids;
  if (GDir* handle = g_dir_open(dir.c_str(), 0, nullptr))
  {
    while (const gchar* id = g_dir_read_name(handle))
    {
      if (g_str_has_prefix(id, (snap + "_").c_str()) &&
          g_str_has_suffix(id, ".desktop"))
      {
        ids.emplace_back(id);
      }
    }
    g_dir_close(handle);
  }

  std::sort(ids.begin(), ids.end());
  for (const auto& id : ids)
  {
    if (auto icon = GetIconForDesktopId(id, {dir}))
    {
      return icon;
    }
  }
  return nullptr;
}

GIcon* GetIconForProcess(int pid)
{
  auto environment = ReadNulSeparated(GetProcPath(pid, "environ"));

  // Set by GIO when launching a desktop file. Child processes inherit it, so
  // only trust it for the process that was launched.
  auto launched = GetEnv(environment, "GIO_LAUNCHED_DESKTOP_FILE");
  if (!launched.empty() &&
      GetEnv(environment, "GIO_LAUNCHED_DESKTOP_FILE_PID") ==
        std::to_string(pid))
  {
    if (auto icon = GetIconForDesktopFile(launched))
    {
      return icon;
    }
  }

  auto flatpakId = ReadKeyFileString(GetProcPath(pid, "root/.flatpak-info"),
                                     "Application", "name");
  if (!flatpakId.empty())
  {
    auto userExports =
      BuildPath(g_get_user_data_dir(), "flatpak/exports/share/applications");
    if (auto icon = GetIconForDesktopId(
          flatpakId + ".desktop",
          {userExports, "/var/lib/flatpak/exports/share/applications"}))
    {
      return icon;
    }
  }

  auto snap = GetEnv(environment, "SNAP_INSTANCE_NAME");
  if (snap.empty())
  {
    snap = GetEnv(environment, "SNAP_NAME");
  }
  if (!snap.empty())
  {
    if (auto icon = GetIconForSnap(snap))
    {
      return icon;
    }
  }

  // A process that isn't part of an installed application has no icon. Its
  // executable's file icon would only be the generic one for its file type.
  auto executables = GetProcessExecutables(pid);
  return executables.empty() ? nullptr
                             : FindDesktopIconForExecutables(executables);
}

template <>
void SystemIconAsyncWorker<ExtensionTag>::Execute()
{
  g_autoptr(GIcon) icon = GetIconForExtension(this->name);
  this->result = RenderIcon(icon, GetPixelSize(this->size));
}

template <>
void SystemIconAsyncWorker<PathTag>::Execute()
{
  g_autoptr(GIcon) icon = GetIconForPath(this->name);
  this->result = RenderIcon(icon, GetPixelSize(this->size));
}

template <>
void SystemIconAsyncWorker<ProcessTag>::Execute()
{
  g_autoptr(GIcon) icon = GetIconForProcess(this->pid);
  this->result = RenderIcon(icon, GetPixelSize(this->size));
}
