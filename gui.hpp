namespace backup_gui
{

    enum Job : int
    {
        Compare = 0,
        Copy,
        Cull
    };

    struct State
    {
        bool is_running = false;
        int job         = Job::Compare;
    };

    void setupGUI(State & state);
} // namespace backup_gui
