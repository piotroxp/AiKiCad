# How to Use the AI Chat Plugin in Eeschema

## Accessing the Chat Panel

The AI Chat Assistant can be triggered in Eeschema in the following ways:

### Method 1: View Menu
1. Open Eeschema
2. Go to **View** → **Panels** → **AI Chat Assistant**
3. The chat panel will appear on the right side of the window

### Method 2: Keyboard Shortcut
- Press **Ctrl+Shift+A** to toggle the AI Chat panel

### Method 3: Panel Close Button
- Once the panel is visible, you can close it using the close button (X) on the panel
- Reopen it using either Method 1 or Method 2

## Panel Location

By default, the AI Chat panel appears on the **right side** of the Eeschema window. You can:
- **Dock it** to any side (left, right, top, bottom)
- **Float it** as a separate window
- **Resize it** by dragging the edges
- **Hide it** using the close button or the menu item

## Using the Chat

1. **Type your command** in the input field at the bottom of the panel
2. **Press Enter** or click **Send** to submit
3. The AI will process your command and respond in the chat history area

### Example Commands

- `add component R1 at 100,200`
- `modify component U1`
- `add trace from 0,0 to 100,100 width 10`

## Integration Points

The plugin is automatically registered when Eeschema starts. The panel is:
- **Hidden by default** - Use the menu or shortcut to show it
- **Persistent** - It remembers its position and size
- **Context-aware** - Understands you're working in the schematic editor

## Troubleshooting

If the panel doesn't appear:
1. Check that the plugin was built successfully
2. Verify the menu item exists: View → Panels → AI Chat Assistant
3. Try the keyboard shortcut: Ctrl+Shift+A
4. Check the console for any error messages
