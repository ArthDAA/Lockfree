#Requires AutoHotkey v2.0
#SingleInstance Force
#UseHook
InstallKeybdHook
SendMode "Input"

; ==============================================================================
; CLASSE DE CONFIGURATION & PARSEUR
; ==============================================================================
class AccentConfig {
    static MinMap := Map()
    static MajMap := Map()
    static AllKeys := Map()
    static CurrentConfigFile := "default.conf" 

    ; Charge la configuration depuis un fichier donné
    static LoadConfig(filepath) {
        if !FileExist(filepath) {
            ; Si default.conf n'existe pas encore, on ne fait rien (le SelectConfigFile s'en chargera)
            return
        }

        ; Réinitialiser les maps
        this.MinMap := Map()
        this.MajMap := Map()
        this.AllKeys := Map()
        
        this.CurrentConfigFile := filepath

        currentSection := ""
        try {
            text := FileRead(filepath, "UTF-8")
        } catch as err {
            MsgBox("Erreur de lecture du fichier : " . err.Message)
            return
        }
        
        Loop Parse, text, "`n", "`r" {
            line := Trim(A_LoopField)
            if (line == "")
                continue

            if (line = "min:") {
                currentSection := "min"
                continue
            }
            else if (line = "maj:") {
                currentSection := "maj"
                continue
            }

            if (InStr(line, "=")) {
                parts := StrSplit(line, "=")
                key := Trim(parts[1])
                valString := Trim(parts[2])
                
                values := []
                Loop Parse, valString, "," {
                    values.Push(Trim(A_LoopField))
                }

                if (currentSection = "min") {
                    this.MinMap[StrLower(key)] := values
                    this.AllKeys[StrLower(key)] := true
                }
                else if (currentSection = "maj") {
                    this.MajMap[StrUpper(key)] := values
                    this.AllKeys[StrLower(key)] := true 
                }
            }
        }
    }

    ; Liste les presets disponibles dans le dossier files/
    static GetAvailableConfigs() {
        configs := []
        Loop Files, "files\*.conf" {
            configs.Push(A_LoopFileName)
        }
        return configs
    }

    ; Affiche le menu pour choisir un preset à appliquer
    static SelectConfigFile() {
        configs := this.GetAvailableConfigs()
        
        if (configs.Length == 0) {
            MsgBox("Aucun fichier .conf trouvé dans le dossier 'files\'!")
            ExitApp
        }

        configGui := Gui("+AlwaysOnTop", "Changer de langue")
        configGui.SetFont("s10")
        
        configGui.Add("Text", "w400", "Choisissez la langue à appliquer :")
        
        ddl := configGui.Add("DropDownList", "w400 vSelectedConfig", configs)
        ddl.Choose(1)
        
        ; Essayer de pré-sélectionner français ou anglais
        for index, file in configs {
            if (InStr(file, "french")) {
                ddl.Choose(index)
                break
            }
        }
        
        configGui.Add("Text", "w400 y+15", "")
        
        btnLoad := configGui.Add("Button", "w195 Default", "&Appliquer")
        ; IMPORTANT : L'appel de méthode doit être correct
        btnLoad.OnEvent("Click", (*) => this.ApplySelectedConfig(configGui, configs))
        
        btnCancel := configGui.Add("Button", "x+10 w195", "&Annuler")
        btnCancel.OnEvent("Click", (*) => configGui.Destroy())
        
        configGui.OnEvent("Close", (*) => configGui.Destroy())
        configGui.OnEvent("Escape", (*) => configGui.Destroy())
        
        configGui.Show()
    }

    ; Copie Source -> default.conf -> Chargement
    static ApplySelectedConfig(guiObj, configs) {
        selectedIndex := guiObj["SelectedConfig"].Value
        if (selectedIndex == 0)
            return

        selectedFile := configs[selectedIndex]
        
        ; La ligne qui posait problème (vérifie bien les guillemets et le point)
        sourcePath := "files\" . selectedFile
        targetPath := "default.conf"
        
        guiObj.Destroy()

        try {
            fileContent := FileRead(sourcePath, "UTF-8")
        } catch as err {
            MsgBox("Erreur lors de la lecture de " . sourcePath . "`n" . err.Message)
            return
        }

        try {
            if FileExist(targetPath)
                FileDelete(targetPath)
            FileAppend(fileContent, targetPath, "UTF-8")
        } catch as err {
            MsgBox("Erreur lors de l'écriture de default.conf`n" . err.Message)
            return
        }
        
        SmartKeyboard.ReloadLogic()
        
        ToolTip("Langue activée : " . selectedFile)
        SetTimer(() => ToolTip(), -2000)
    }
}

; ==============================================================================
; MOTEUR
; ==============================================================================
class SmartKeyboard {
    static CurrentKey := ""
    static CurrentIndex := 0
    static IsActive := false

    static Init() {
        for key, _ in AccentConfig.AllKeys {
            Hotkey(">!*" . key, this.HandlePress.Bind(this, key), "On")
        }

        Hotkey("~*RAlt Up", (*) => this.CleanupAndMaskMenu(), "On")
        Hotkey("~*LButton", (*) => this.ResetState(), "On")
        Hotkey("~*Esc", (*) => this.ResetState(), "On")
        
        ; Raccourci pour changer de langue : Ctrl+Shift+Alt+R
        Hotkey("^+!r", (*) => AccentConfig.SelectConfigFile(), "On")
    }

    static HandlePress(baseKey, *) {
        isShift := GetKeyState("Shift", "P")
        isCaps := GetKeyState("CapsLock", "T")
        isUpperCase := (isShift != isCaps)

        targetList := ""

        if (isUpperCase) {
            keyUpper := StrUpper(baseKey)
            if (AccentConfig.MajMap.Has(keyUpper))
                targetList := AccentConfig.MajMap[keyUpper]
        } else {
            keyLower := StrLower(baseKey)
            if (AccentConfig.MinMap.Has(keyLower))
                targetList := AccentConfig.MinMap[keyLower]
        }

        if (!targetList)
            return

        Critical("On") 

        if (this.CurrentKey != baseKey) {
            this.CurrentKey := baseKey
            this.CurrentIndex := 1
        } else {
            Send("{Backspace}")
            this.CurrentIndex++
            if (this.CurrentIndex > targetList.Length)
                this.CurrentIndex := 1
        }

        charToSend := targetList[this.CurrentIndex]
        Send("{Text}" . charToSend)
        
        this.UpdateTooltip(targetList, this.CurrentIndex)
        this.IsActive := true
        
        Critical("Off")
    }

    static UpdateTooltip(variants, activeIndex) {
        displayText := ""
        for index, char in variants {
            if (index == activeIndex)
                displayText .= "[" . char . "] "
            else
                displayText .= char . " "
        }
        
        ; On essaie de trouver le vrai curseur texte
        if (this.GetCaretPosEx(&cX, &cY)) {
            ; Curseur trouvé : on affiche juste en dessous
            CoordMode "ToolTip", "Screen"
            ToolTip(displayText, cX, cY + 30)
        } else {
            ; Curseur INTROUVABLE (Chrome, Electron...)
            ; Au lieu de suivre la souris, on affiche en bas au centre de la FENÊTRE active.
            ; C'est beaucoup plus propre, comme des sous-titres.
            try {
                WinGetPos(&wX, &wY, &wW, &wH, "A")
                CoordMode "ToolTip", "Screen"
                ; Position : Centre horizontal de la fenêtre, en bas (marge de 50px)
                ToolTip(displayText, wX + (wW / 2) - 50, wY + wH - 60)
            } catch {
                ; Si vraiment tout rate (pas de fenêtre ?), on fallback sur la souris
                ToolTip(displayText)
            }
        }
    }

    ; --- Nouvelle fonction magique pour mieux trouver le curseur ---
    static GetCaretPosEx(&cX, &cY) {
        ; 1. Essai standard AHK (Marche dans Notepad, GUI natifs)
        if CaretGetPos(&cX, &cY)
            return true
            
        ; 2. Essai via l'API Windows GUIThreadInfo (Marche parfois dans Word/Excel)
        static GUITHREADINFO_SIZE := 48 + A_PtrSize * 6
        guiThreadInfo := Buffer(GUITHREADINFO_SIZE, 0)
        NumPut("UInt", GUITHREADINFO_SIZE, guiThreadInfo)
        
        if DllCall("GetGUIThreadInfo", "UInt", 0, "Ptr", guiThreadInfo.Ptr) {
            flags := NumGet(guiThreadInfo, 4, "UInt")
            if (flags & 1) { ; GUI_CARETBLINKING
                hwnd := NumGet(guiThreadInfo, 8 + A_PtrSize, "Ptr")
                rcLeft := NumGet(guiThreadInfo, 8 + A_PtrSize * 2 + 12, "Int")
                rcTop := NumGet(guiThreadInfo, 8 + A_PtrSize * 2 + 16, "Int")
                
                if (hwnd) {
                    pt := Buffer(8)
                    NumPut("Int", rcLeft, "Int", rcTop, pt)
                    DllCall("ClientToScreen", "Ptr", hwnd, "Ptr", pt)
                    cX := NumGet(pt, 0, "Int")
                    cY := NumGet(pt, 4, "Int")
                    return true
                }
            }
        }
        
        ; 3. Échec total (Applications Electron type Discord, VSCode, Slack...)
        return false
    }

    static CleanupAndMaskMenu() {
        if (this.IsActive) {
            this.ResetState()
            Send("{Blind}{vk07}")
        }
    }

    static ResetState() {
        this.CurrentKey := ""
        this.CurrentIndex := 0
        this.IsActive := false
        ToolTip()
    }

    static ReloadLogic() {
        for key, _ in AccentConfig.AllKeys {
            try Hotkey(">!*" . key, "Off")
        }
        AccentConfig.LoadConfig("default.conf")
        for key, _ in AccentConfig.AllKeys {
            Hotkey(">!*" . key, this.HandlePress.Bind(this, key), "On")
        }
    }

    static ShowHelp() {
        helpGui := Gui("+AlwaysOnTop", "Bienvenue sur LockFree v0.1")
        helpGui.SetFont("s11", "Segoe UI")
        helpGui.BackColor := "White"
        
        ; Titre
        helpGui.SetFont("s16 bold c0078D7")
        helpGui.Add("Text", "w500 Center", "Comment utiliser LockFree ?")
        
        ; Tuto
        helpGui.SetFont("s11 norm cBlack")
        helpGui.Add("Text", "x20 w480", "1. Maintenez la touche Alt Gr (Alt de droite) enfoncée.")
        helpGui.Add("Text", "x20 w480 y+5", "2. Appuyez plusieurs fois sur une lettre (ex: 'e').")
        helpGui.Add("Text", "x20 w480 y+5", "3. Relâchez Alt Gr pour valider.")
        
        ; Séparateur
        helpGui.Add("Text", "x10 w480 h2 0x10") ; Ligne horizontale
        
        ; Commandes
        helpGui.SetFont("s11 bold")
        helpGui.Add("Text", "x20 y+10", "Raccourcis :")
        helpGui.SetFont("s10 norm")
        helpGui.Add("Text", "x30 y+5", "• Ctrl + Shift + Alt + R : Changer de langue / Recharger")
        helpGui.Add("Text", "x30 y+5", "• Echap : Annuler la sélection en cours")
        
        ; Bouton fermer
        btn := helpGui.Add("Button", "w100 x200 y+20 Default", "J'ai compris !")
        btn.OnEvent("Click", (*) => helpGui.Destroy())
        
        helpGui.Show()
    }
}

; ==============================================================================
; DÉMARRAGE
; ==============================================================================
isFirstRun := !FileExist("default.conf")

if !isFirstRun {
    AccentConfig.LoadConfig("default.conf")
    SmartKeyboard.Init()
    A_IconTip := "Smart Keyboard - Prêt (default.conf)"
} 
else {
    ; Premier lancement
    MsgBox("Bienvenue ! Veuillez choisir votre langue principale.")
    AccentConfig.SelectConfigFile()
    ; On affiche l'aide après la sélection
    SmartKeyboard.ShowHelp()
}
