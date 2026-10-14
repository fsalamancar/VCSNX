using System.Globalization;

namespace Vcsnx.Builder.Core;

/// <summary>Every text the player reads, in Spanish (Windows in Spanish) and English (anything else).</summary>
public static class Strings
{
    static readonly Dictionary<string, (string Es, string En)> Table = new(StringComparer.Ordinal)
    {
        ["GameMissing"] = ("No encontré el ejecutable del juego. Elige la carpeta PSP_DATA (la que tiene PSP_GAME).",
                           "No game executable found. Pick the PSP_DATA folder (the one with PSP_GAME)."),
        ["GameEncrypted"] = ("El ejecutable está cifrado. Prepara tu carpeta PSP_DATA como explica el port de PC (VCSNative): el ejecutable desencriptado va en PSP_GAME\\SYSDIR\\EBOOT_DECRYPTED.ELF.",
                             "The executable is encrypted. Prepare your PSP_DATA folder as the PC port (VCSNative) explains: the decrypted executable goes in PSP_GAME\\SYSDIR\\EBOOT_DECRYPTED.ELF."),
        ["GameUnknown"] = ("Esta copia no es GTA Vice City Stories ULUS-10160 (EE. UU.). Solo esa versión funciona.",
                           "This copy is not GTA Vice City Stories ULUS-10160 (US). Only that version works."),
        ["GameOk"] = ("✓ GTA Vice City Stories ULUS-10160 {0}", "✓ GTA Vice City Stories ULUS-10160 {0}"),
        ["PathNotAscii"] = ("Mueve la carpeta del programa a una ruta sin acentos ni caracteres especiales, por ejemplo C:\\VCSNX-Builder (ahora está en {0}).",
                            "Move the program's folder to a path without accents or special characters, for example C:\\VCSNX-Builder (it is now in {0})."),
        ["NotEnoughSpace"] = ("Faltan {0} GB libres en el disco de la carpeta del programa.", "{0} GB more free space is needed on the program's drive."),
        ["DownloadFailed"] = ("No pude bajar el código traducido ({0}). Revisa tu conexión, o elige el ZIP de PSPRecomp ya bajado.",
                              "Could not download the translated code ({0}). Check your connection, or pick an already downloaded PSPRecomp ZIP."),
        ["CorpusMismatch"] = ("El código traducido no coincide con el esperado. Vuelve a intentarlo más tarde.",
                              "The translated code does not match what is expected. Try again later."),
        ["CompileFailed"] = ("Falló la compilación de {0}. El detalle está en salida\\builder.log.", "Compiling {0} failed. Details are in salida\\builder.log."),
        ["ToolBlocked"] = ("Windows no dejó abrir {0} ({1}). Si tu antivirus lo bloqueó, permite la carpeta del programa y vuelve a pulsar \"Crear\": lo ya compilado se conserva.",
                           "Windows did not let {0} start ({1}). If your antivirus blocked it, allow the program's folder and press \"Create\" again: what was already compiled is kept."),
        ["Unexpected"] = ("Algo salió mal: {0}. El detalle está en salida\\builder.log.", "Something went wrong: {0}. Details are in salida\\builder.log."),
        ["ToolFailed"] = ("Falló {0}. El detalle está en salida\\builder.log.", "{0} failed. Details are in salida\\builder.log."),
        ["SdCopyFailed"] = ("No pude copiar a la SD ({0}). Los archivos están en {1}: cópialos a mano o vuelve a intentarlo.",
                            "Could not copy to the SD card ({0}). The files are in {1}: copy them by hand or try again."),
        ["IconUnreadable"] = ("No pude leer esa imagen. Usa un PNG o JPG.", "Could not read that image. Use a PNG or JPG."),
        ["Done"] = ("Listo: vcs_game.so y vcsnx.nro están en {0}. Cópialos a switch\\vcsnx\\ en tu SD y abre VCSNX manteniendo R al abrir un juego.",
                    "Done: vcs_game.so and vcsnx.nro are in {0}. Copy them to switch\\vcsnx\\ on your SD card and open VCSNX holding R while starting a game."),
        ["DoneSd"] = ("Listo: vcs_game.so y vcsnx.nro ya están en tu SD (y en {0}). Abre VCSNX manteniendo R al abrir un juego.",
                      "Done: vcs_game.so and vcsnx.nro are on your SD card (and in {0}). Open VCSNX holding R while starting a game."),
        ["StepGame"] = ("1. Tu juego (carpeta PSP_DATA)", "1. Your game (PSP_DATA folder)"),
        ["StepIcon"] = ("2. Ícono (opcional)", "2. Icon (optional)"),
        ["StepSd"] = ("3. SD de la Switch (opcional)", "3. Switch SD card (optional)"),
        ["StepHd"] = ("4. Texturas y videos HD (opcional): tu carpeta Textures", "4. HD textures and movies (optional): your Textures folder"),
        ["Choose"] = ("Elegir", "Choose"),
        ["ChooseImage"] = ("Elegir imagen…", "Choose image…"),
        ["Remove"] = ("Quitar", "Remove"),
        ["NoSd"] = ("Ninguna", "None"),
        ["Create"] = ("Crear vcs_game.so", "Create vcs_game.so"),
        ["Cancel"] = ("Cancelar", "Cancel"),
        ["Cancelled"] = ("Cancelado. Lo ya hecho se guarda para la próxima vez.", "Cancelled. What is done is kept for next time."),
        ["Details"] = ("Ver detalles", "Show details"),
        ["Remaining"] = ("Tiempo restante: ~{0} min", "Time left: ~{0} min"),
        ["StageChecking"] = ("Revisando…", "Checking…"),
        ["StageDownloading"] = ("Bajando el código traducido…", "Downloading the translated code…"),
        ["StageCompiling"] = ("Compilando el juego {0}/{1}", "Compiling the game {0}/{1}"),
        ["StageLinking"] = ("Enlazando vcs_game.so…", "Linking vcs_game.so…"),
        ["StagePackaging"] = ("Preparando vcsnx.nro…", "Preparing vcsnx.nro…"),
        ["StageCopyingToSd"] = ("Copiando a la SD…", "Copying to the SD card…"),
        ["HdFolderEmpty"] = ("La carpeta HD {0} no tiene texturas (archivos como 0123456789abcdef.png) ni videos (LOGO.mp4, TITLES.mp4). Elige tu carpeta Textures, o déjala vacía para armar el juego sin HD.",
                             "The HD folder {0} has no textures (files like 0123456789abcdef.png) and no movies (LOGO.mp4, TITLES.mp4). Pick your Textures folder, or leave it empty to build the game without HD."),
        ["DoneHd"] = ("HD: {0} texturas y {1} videos en {2} (carpetas Textures y HD). Cópialas a switch\\vcsnx\\ en tu SD.",
                      "HD: {0} textures and {1} movies in {2} (Textures and HD folders). Copy them to switch\\vcsnx\\ on your SD card."),
        ["DoneHdSd"] = ("HD: {0} texturas y {1} videos copiados a tu SD ({2} ya estaban).",
                        "HD: {0} textures and {1} movies copied to your SD card ({2} were already there)."),
        ["StageDone"] = ("Listo", "Done"),
    };

    public static string Get(string key, params object[] args) => Get(CultureInfo.CurrentUICulture, key, args);

    public static string Get(CultureInfo culture, string key, params object[] args)
    {
        var (es, en) = Table[key];
        return string.Format(culture, culture.TwoLetterISOLanguageName == "es" ? es : en, args);
    }
}

/// <summary>A problem the player can act on; Message is English (for logs), Localized is for the window.</summary>
public sealed class BuilderException : Exception
{
    public BuilderException(string key, params object[] args) : base(Strings.Get(CultureInfo.InvariantCulture, key, args))
    {
        Key = key;
        Args = args;
    }

    public string Key { get; }
    public object[] Args { get; }
    public string Localized => Strings.Get(Key, Args);
}
