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
  // Applications registered with Launch Services know their own icon. One
  // without a bundle would only have the generic icon for executables.
  auto app = [NSRunningApplication
    runningApplicationWithProcessIdentifier:static_cast<pid_t>(this->pid)];
  NSImage* image = app != nil && app.bundleURL != nil ? app.icon : nil;

  if (image == nil)
  {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(static_cast<pid_t>(this->pid), path, sizeof(path)) <= 0)
    {
      return;
    }

    // The innermost .app bundle containing the executable. A process outside
    // of one has no icon.
    NSString* bundle = nil;
    for (auto dir = [NSString stringWithUTF8String:path]; dir.length > 1;
         dir = [dir stringByDeletingLastPathComponent])
    {
      if ([[dir pathExtension] isEqualToString:@"app"])
      {
        bundle = dir;
        break;
      }
    }
    if (bundle == nil)
    {
      return;
    }

    image = [[NSWorkspace sharedWorkspace] iconForFile:bundle];
  }

  [image setSize:GetSize(this->size)];

  if (image.valid)
  {
    this->result = ImageToPng(image);
  }
}
