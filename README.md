# AssetsBridge

![AssetsBridge](Plugins/AssetsBridge/AssetsBridge.png)

**Bidirectional asset bridge between Blender and Unreal Engine.**

AssetsBridge enables seamless round-trip asset workflows between Blender and Unreal Engine. Export meshes from Unreal, modify them in Blender, and send them back—preserving materials, transforms, skeleton references, and morph targets.

## Sister Project: Blender Addon

This Unreal Engine plugin works in tandem with the **AssetsBridge Blender Addon**:

🔗 **[AssetsBridge Blender Addon](https://github.com/nitecon/assetsbridge-addon)**

Both components are required for the full workflow:
| Component | Purpose |
|-----------|---------|
| **This Plugin** (Unreal Engine) | Export assets to glTF, read modified assets back, manage UE-side integration |
| **Blender Addon** | Import assets from Unreal, edit meshes/skeletons/shape keys, export back |

## Features

### Unreal Engine Plugin
- **Export to Blender** - Export Static Meshes and Skeletal Meshes to glTF format
- **Import from Blender** - Read modified assets back with preserved metadata
- **Material Tracking** - Maintains material assignments and slot order
- **Transform Preservation** - Keeps world position, rotation, and scale
- **Morph Target Support** - Exports and reimports blend shapes/morph targets
- **Skeleton References** - Preserves skeleton paths for skeletal mesh reimport

### Blender Addon
- **Bidirectional JSON Protocol** - `from-unreal.json` and `from-blender.json` coordinate transfers
- **Mesh Tools** - Split meshes, assign UE5 skeletons, set export paths
- **Shape Key Transfer** - Selective transfer between meshes
- **Collection Hierarchy** - Mirrors Unreal content browser structure
- **Automatic Scene Configuration** - Sets up Blender for Unreal-compatible units

## Installation

### Unreal Engine Plugin

#### Option 1: Download Release (Recommended)
1. Download the zip for your UE version from [Releases](https://github.com/nitecon/AssetsBridge/releases)
2. Extract to `YourProject/Plugins/AssetsBridge`
3. Open your project in Unreal Engine
4. Enable the plugin: **Edit → Plugins → Search "AssetsBridge" → Enable**
5. Restart the editor when prompted

#### Option 2: Clone Repository
```bash
cd YourProject/Plugins
git clone https://github.com/nitecon/AssetsBridge.git
```

### Blender Addon
See installation instructions at [assetsbridge-addon](https://github.com/nitecon/assetsbridge-addon)

## Configuration

### First-Time Setup

1. **Configure Bridge Directory**
   - In Unreal: Click the AssetsBridge settings icon in the toolbar
   - Set the bridge directory (shared folder between Blender and UE)
   
2. **Configure Blender Addon**
   - In Blender: **Edit → Preferences → Add-ons → AssetsBridge**
   - Point to the same bridge directory

The bridge directory is where JSON files and exported glTF assets are stored for transfer between applications.

## Usage Workflow

### Unreal → Blender (Export)
1. Select assets in the Content Browser
2. Right-click → **AssetsBridge → Export to Blender**
3. Assets are exported to the bridge directory
4. In Blender: Click **Import Objects** in the AssetsBridge panel

### Blender → Unreal (Import)
1. Make your modifications in Blender
2. Select modified objects
3. Click **Export Selected** in the AssetsBridge panel
4. In Unreal: **AssetsBridge → Import from Blender**
5. Assets are reimported with changes applied

### Mesh Tools (Blender)
- **Split to New Mesh** - Separate faces into new wearable pieces
- **Set Export Path** - Configure Unreal destination path
- **Assign UE5 Skeleton** - Reuse existing skeleton on reimport
- **Shape Key Transfer** - Copy morph targets between meshes

## Supported Asset Types

| Type | Export | Import | Notes |
|------|--------|--------|-------|
| Static Mesh | ✅ | ✅ | Full support with materials |
| Skeletal Mesh | ✅ | ✅ | Includes skeleton and weights |
| Morph Targets | ✅ | ✅ | Shape keys preserved by name |
| Materials | ✅ | ✅ | Material slots tracked |

## Requirements

### Unreal Engine
- **Unreal Engine 5.4+**
- **DatasmithImporter** plugin (included with UE)
- **EditorScriptingUtilities** plugin (included with UE)

### Blender
- **Blender 4.0+** (5.0+ recommended)

## Troubleshooting

### Assets not appearing in Blender
- Verify both applications point to the same bridge directory
- Check that `from-unreal.json` exists in the bridge directory
- Ensure Blender scene units are set to Metric with 0.01 scale

### Materials lost on reimport
- Material slots must maintain the same order
- New materials are assigned the default WorldGridMaterial

### Skeleton issues on reimport
- Use **Assign UE5 Skeleton** in Blender to specify existing skeleton path
- Ensure bone hierarchy matches the original

## Support

If you find this project useful, consider supporting development:

☕ **[Buy me a coffee](https://buymeacoffee.com/nitecon)**

## License

This project is open source under the GPL license.

---

**Made with ❤️ by [Nitecon Studios LLC](https://github.com/nitecon)**
