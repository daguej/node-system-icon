#include "system_icon.hpp"
#import <AppKit/AppKit.h>
#include <libproc.h>

std::vector<unsigned char> ImageToPng(NSImage* image)
{
  auto imageRef = [image CGImageForProposedRect:nil context:nil hints:nil];
  auto imageRep = [[NSBitmapImageRep alloc] initWithCGImage:imageRef];
  [imageRep setSize:[image size]];
  auto imageData =
    [imageRep representationUsingType:NSBitmapImageFileTypePNG
                           properties:[[NSDictionary alloc] init]];
  auto p = static_cast<const unsigned char*>([imageData bytes]);
  return std::vector<unsigned char>{p, p + [imageData length]};
}

NSSize GetSize(IconSize size)
{
  switch (size)
  {
    case IconSize::ExtraSmall:
      return NSMakeSize(16, 16);
    case IconSize::Small:
      return NSMakeSize(32, 32);
    case IconSize::Medium:
      return NSMakeSize(128, 128);
    case IconSize::Large:
      return NSMakeSize(256, 256);
    case IconSize::ExtraLarge:
      return NSMakeSize(512, 512);
  }
}

template <>
void SystemIconAsyncWorker<ExtensionTag>::Execute()
{
  auto image = [[NSWorkspace sharedWorkspace]
    iconForFileType:[NSString stringWithUTF8String:this->name.c_str()]];
  [image setSize:GetSize(this->size)];

  if (image.valid)
  {
    this->result = ImageToPng(image);
  }
}

template <>
void SystemIconAsyncWorker<PathTag>::Execute()
{
  auto image = [[NSWorkspace sharedWorkspace]
    iconForFile:[NSString stringWithUTF8String:this->name.c_str()]];
  [image setSize:GetSize(this->size)];

  if (image.valid)
  {
    this->result = ImageToPng(image);
  }
}

template <>
void SystemIconAsyncWorker<ProcessTag>::Execute()
{
  // Applications registered with Launch Services know their own bundle, even
  // when their executable is an interpreter living elsewhere. For any other
  // process, start from its executable.
  auto app = [NSRunningApplication
    runningApplicationWithProcessIdentifier:static_cast<pid_t>(this->pid)];
  NSString* path = app.bundleURL.path;
  if (path == nil)
  {
    char buf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(static_cast<pid_t>(this->pid), buf, sizeof(buf)) <= 0)
    {
      return;
    }
    path = [NSString stringWithUTF8String:buf];
  }

  // The outermost .app bundle containing it, so that helpers nested inside an
  // application, such as Chrome's or an Electron app's, get its icon. A
  // process outside of one, including an unbundled executable that checked in
  // with Launch Services, would only have the generic icon, so it has none.
  NSString* bundle = nil;
  for (auto dir = path; dir.length > 1;
       dir = [dir stringByDeletingLastPathComponent])
  {
    if ([[dir pathExtension] isEqualToString:@"app"])
    {
      bundle = dir;
    }
  }
  if (bundle == nil)
  {
    return;
  }

  auto image = [[NSWorkspace sharedWorkspace] iconForFile:bundle];
  [image setSize:GetSize(this->size)];

  if (image.valid)
  {
    this->result = ImageToPng(image);
  }
}
