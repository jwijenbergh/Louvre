#ifndef LLOG_H
#define LLOG_H

#include <LNamespaces.h>

/**
 * @brief Debugging information
 *
 * The LLog class provides an output stream for debugging information.\n
 * The level of verbosity can be assigned setting the **LOUVRE_DEBUG** environment variable to a non negative integer value.\n
 *
 * ## Verbosity levels
 *
 * #### LOUVRE_DEBUG=0
 * Disables messages except those generated by log().
 *
 * #### LOUVRE_DEBUG=1
 * Prints messages generated by log() and fatal().
 *
 * #### LOUVRE_DEBUG=2
 * Prints messages generated by log(), fatal() and error().
 *
 * #### LOUVRE_DEBUG=3
 * Prints messages generated by log(), fatal(), error() and warning().
 *
 * #### LOUVRE_DEBUG=4
 * Prints messages generated by log(), fatal(), error(), warning() and debug().
 */
class Louvre::LLog
{
public:
    /**
     * Call this method to print messages before creating an LCompositor instance.
     */
    static void init();

    /// Prints general messages independent of the value of **LOUVRE_DEBUG**.
    static void log(const char *format, ...);

    /// Reports an unrecoverable error. **LOUVRE_DEBUG** >= 1.
    static void fatal(const char *format, ...);

    /// Reports a nonfatal error. **LOUVRE_DEBUG** >= 2.
    static void error(const char *format, ...);

    /// Messages that report a risk for the compositor. **LOUVRE_DEBUG** >= 3.
    static void warning(const char *format, ...);

    /// Debugging messages. **LOUVRE_DEBUG** >= 4.
    static void debug(const char *format, ...);
};

#endif // LLOG_H
