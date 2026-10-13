using Vcsnx.Builder.Core;

namespace Vcsnx.Builder.App;

/// <summary>The window (spec §4.2): it only gathers the three choices and shows the pipeline's progress.</summary>
internal sealed class MainForm : Form
{
    const string Title = "GTA VCS Compiler";

    readonly BuildPaths _paths = BuildPaths.NextTo(AppContext.BaseDirectory);
    readonly TextBox _game = new() { Width = 340 };
    readonly Label _gameStatus = new() { AutoSize = true, MaximumSize = new Size(440, 0) };
    readonly PictureBox _iconPreview = new() { Size = new Size(96, 96), SizeMode = PictureBoxSizeMode.Zoom, BorderStyle = BorderStyle.FixedSingle };
    readonly ComboBox _sd = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260 };
    readonly Button _create = new() { AutoSize = true, Text = Strings.Get("Create"), Enabled = false };
    readonly ProgressBar _bar = new() { Width = 440, Maximum = 1000 };
    readonly Label _stage = new() { AutoSize = true };
    readonly Label _remaining = new() { AutoSize = true };
    readonly Button _cancel = new() { AutoSize = true, Text = Strings.Get("Cancel"), Enabled = false };
    readonly LinkLabel _details = new() { AutoSize = true, Text = "▸ " + Strings.Get("Details") };
    readonly TextBox _log = new() { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Size = new Size(440, 160), Visible = false, Font = new Font(FontFamily.GenericMonospace, 8) };
    string? _icon;
    CancellationTokenSource? _run;

    public MainForm()
    {
        Text = Title;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        AutoSize = true;
        AutoSizeMode = AutoSizeMode.GrowAndShrink;
        var header = new Label { Text = Title, Font = new Font("Segoe UI", 24, FontStyle.Bold), ForeColor = Color.White, BackColor = Color.Black, TextAlign = ContentAlignment.MiddleCenter, Size = new Size(460, 64) };
        var chooseGame = new Button { Text = Strings.Get("Choose"), AutoSize = true };
        var chooseIcon = new Button { Text = Strings.Get("ChooseImage"), AutoSize = true };
        var removeIcon = new Button { Text = Strings.Get("Remove"), AutoSize = true };
        var layout = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Padding = new Padding(12), WrapContents = false };
        layout.Controls.AddRange([
            header,
            new Label { Text = Strings.Get("StepGame"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, Row(_game, chooseGame), _gameStatus,
            new Label { Text = Strings.Get("StepIcon"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, Row(chooseIcon, removeIcon, _iconPreview),
            new Label { Text = Strings.Get("StepSd"), AutoSize = true, Margin = new Padding(0, 12, 0, 0) }, _sd,
            Row(_create), _stage, _bar, Row(_remaining, _cancel), _details, _log,
            new Label { Text = "bAlo productions", AutoSize = true, ForeColor = Color.Gray, Margin = new Padding(0, 12, 0, 0) },
        ]);
        Controls.Add(layout);

        chooseGame.Click += (_, _) =>
        {
            using var dialog = new FolderBrowserDialog { ShowNewFolderButton = false };
            if (dialog.ShowDialog(this) == DialogResult.OK) _game.Text = dialog.SelectedPath;
        };
        _game.TextChanged += async (_, _) => await CheckGameAsync();
        chooseIcon.Click += (_, _) =>
        {
            using var dialog = new OpenFileDialog { Filter = "PNG / JPG|*.png;*.jpg;*.jpeg" };
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            _icon = dialog.FileName;
            _iconPreview.ImageLocation = _icon;
        };
        removeIcon.Click += (_, _) => { _icon = null; _iconPreview.ImageLocation = null; _iconPreview.Image = null; };
        _sd.DropDown += (_, _) => FillSdList();
        _create.Click += async (_, _) => await CreateAsync();
        _cancel.Click += (_, _) => _run?.Cancel();
        _details.LinkClicked += (_, _) => _log.Visible = !_log.Visible;
        FillSdList();
    }

    static FlowLayoutPanel Row(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
        row.Controls.AddRange(controls);
        return row;
    }

    void FillSdList()
    {
        var current = (_sd.SelectedItem as SdChoice)?.Root;
        _sd.Items.Clear();
        _sd.Items.Add(new SdChoice(null, Strings.Get("NoSd")));
        foreach (var target in SdCard.Candidates()) _sd.Items.Add(new SdChoice(target.Root, target.Label));
        _sd.SelectedItem = _sd.Items.Cast<SdChoice>().FirstOrDefault(c => c.Root == current) ?? _sd.Items[0];
    }

    sealed record SdChoice(string? Root, string Label)
    {
        public override string ToString() => Label;
    }

    async Task CheckGameAsync()
    {
        var folder = _game.Text;
        GameCheck check;
        try
        {
            var known = HashList.Load(Path.Combine(_paths.Hashes, "executable.sha256")).Values.ToHashSet(StringComparer.Ordinal);
            check = await Task.Run(() => GameCopy.Check(folder, known));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            _gameStatus.ForeColor = Color.DarkRed;
            _gameStatus.Text = e.Message;
            _create.Enabled = false;
            return;
        }
        if (folder != _game.Text) return; // the player kept typing
        _gameStatus.ForeColor = check.Ok ? Color.DarkGreen : Color.DarkRed;
        _gameStatus.Text = check.Ok ? Strings.Get("GameOk", check.DiscVersion ?? "")
            : Strings.Get(check.State switch { ExecutableState.Missing => "GameMissing", ExecutableState.Encrypted => "GameEncrypted", _ => "GameUnknown" });
        _create.Enabled = check.Ok;
    }

    async Task CreateAsync()
    {
        _run = new CancellationTokenSource();
        _create.Enabled = false;
        _cancel.Enabled = true;
        _log.Clear();
        Directory.CreateDirectory(_paths.Output);
        await using var logFile = new StreamWriter(Path.Combine(_paths.Output, "builder.log"), append: false) { AutoFlush = true };
        var log = TextWriter.Synchronized(new Tee(logFile, line => BeginInvoke(new Action(() => _log.AppendText(line + Environment.NewLine)))));
        var runner = new LoggingRunner(new SystemProcessRunner(Path.Combine(Path.GetFullPath(_paths.Work), "tmp")), log);
        var pipeline = new Pipeline(_paths, new HttpClient(), runner, Environment.ProcessorCount,
            GC.GetGCMemoryInfo().TotalAvailableMemoryBytes, path => new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))!).AvailableFreeSpace);
        var progress = new Progress<BuildProgress>(ShowProgress);
        try
        {
            var request = new BuildRequest(_game.Text, _icon, (_sd.SelectedItem as SdChoice)?.Root);
            var result = await RunWithCorpusFallbackAsync(pipeline, request, progress, log);
            MessageBox.Show(this, Strings.Get(result.Sd is null ? "Done" : "DoneSd", Path.GetDirectoryName(result.Module)!), Title);
        }
        catch (BuilderException e) { log.WriteLine(e.Message); MessageBox.Show(this, e.Localized, Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); }
        catch (CompileFailedException e) { log.WriteLine(e.Message); MessageBox.Show(this, Strings.Get("CompileFailed", e.Unit), Title, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        catch (ToolFailedException e) { log.WriteLine(e.Message); MessageBox.Show(this, Strings.Get("ToolFailed", e.Tool), Title, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        catch (OperationCanceledException) when (_run.IsCancellationRequested) { _stage.Text = Strings.Get("Cancelled"); }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            log.WriteLine(e.ToString());
            MessageBox.Show(this, e.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch (Exception e) // anything else still reaches the log and the player, never WinForms' crash dialog
        {
            log.WriteLine(e.ToString());
            MessageBox.Show(this, Strings.Get("Unexpected", e.Message), Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            _cancel.Enabled = false;
            _create.Enabled = true;
            _run.Dispose();
            _run = null;
        }
    }

    // Without internet the player can point at a PSPRecomp ZIP downloaded elsewhere (spec §5).
    async Task<BuildResult> RunWithCorpusFallbackAsync(Pipeline pipeline, BuildRequest request, IProgress<BuildProgress> progress, TextWriter log)
    {
        try
        {
            return await pipeline.RunAsync(request, progress, log, _run!.Token);
        }
        catch (BuilderException e) when (e.Key == "DownloadFailed")
        {
            if (MessageBox.Show(this, e.Localized, Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) throw;
            using var dialog = new OpenFileDialog { Filter = "ZIP|*.zip" };
            if (dialog.ShowDialog(this) != DialogResult.OK) throw;
            return await pipeline.RunAsync(request with { CorpusZip = dialog.FileName }, progress, log, _run!.Token);
        }
    }

    void ShowProgress(BuildProgress p)
    {
        _stage.Text = p.Stage == Stage.Compiling ? Strings.Get("StageCompiling", p.Done, p.Total) : Strings.Get("Stage" + p.Stage);
        _bar.Value = p.Stage switch
        {
            Stage.Checking => 0,
            Stage.Downloading => 20,
            Stage.Compiling => 50 + (p.Total == 0 ? 0 : 850 * p.Done / p.Total),
            Stage.Linking => 920,
            Stage.Packaging => 960,
            Stage.CopyingToSd => 980,
            _ => 1000,
        };
        _remaining.Text = p.Remaining is { } r && p.Stage == Stage.Compiling ? Strings.Get("Remaining", Math.Max(1, (int)Math.Ceiling(r.TotalMinutes))) : "";
    }

    /// <summary>Writes the log file and shows each line in "Show details".</summary>
    sealed class Tee(TextWriter file, Action<string> show) : TextWriter
    {
        public override System.Text.Encoding Encoding => file.Encoding;
        public override void Write(char value) => file.Write(value);
        public override void WriteLine(string? value)
        {
            file.WriteLine(value);
            show(value ?? "");
        }
    }
}
