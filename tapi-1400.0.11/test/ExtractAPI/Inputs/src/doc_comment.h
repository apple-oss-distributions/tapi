/**
 * This is an unattached doc comment.
 */

/// This is a long multiline doc comment.
///   Parts of it might be indented.
///
/// ^ blank lines are kept.
/** The comments styles might be mixed. */
//! \brief It might contain comment commands.
extern int foo();

// This is a normal comment.
extern int bar();

enum {
  baz, ///< This is a one line trailing doc comment.
};
