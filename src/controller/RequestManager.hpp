// This class should own a message worker thread, and should expose an async
// portRequest method. GUI should only call this async method, which enques a
// request send and registers the request into an in-flight requests vector.
// requests are either timed out or completed with a reply or an error. The
// RequestManager class handles both, and launches gui updates (async)
