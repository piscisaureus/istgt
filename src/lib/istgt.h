#ifndef ISTGT_H
#define ISTGT_H

#ifndef istgt_export
#ifdef _WIN32
#define istgt_export __declspec(dllimport)
#else // _WIN32
#define istgt_export
#endif // _WIN32
#endif

istgt_export int istgt_start();

#endif  // ISTGT_H
