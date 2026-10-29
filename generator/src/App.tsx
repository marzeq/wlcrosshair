import { useEffect, useMemo, useRef, useState, type DragEvent } from "react"
import { flushSync } from "react-dom"
import { canvasSize, preloadImages, render } from "./render"
import {
  clamp,
  decodeShare,
  encodeShare,
  initialLayers,
  newLayer,
  typeNames,
  type Layer,
  type LayerType,
} from "./model"

const kinds: LayerType[] = ["dot", "cross", "ring", "chevron", "image"]
const maxDimension = 2048
const assetBase = import.meta.env.BASE_URL
const scenes = [
  {
    id: "cs2",
    label: "CS2 · Dust II",
    image: `${assetBase}backgrounds/cs2-dust2.webp`,
  },
  {
    id: "valorant",
    label: "VALORANT · Lotus",
    image: `${assetBase}backgrounds/valorant-lotus.jpg`,
  },
  {
    id: "deadlock",
    label: "Deadlock",
    image: `${assetBase}backgrounds/deadlock.jpg`,
  },
] as const

type NumberKey = NonNullable<
  { [K in keyof Layer]: Layer[K] extends number ? K : never }[keyof Layer]
>

function NumberField({
  label,
  value,
  min = 0,
  max = maxDimension,
  step = 1,
  suffix = "px",
  onChange,
}: {
  label: string
  value: number
  min?: number
  max?: number
  step?: number
  suffix?: string
  onChange: (value: number) => void
}) {
  const [draft, setDraft] = useState(String(value))
  const allowEmpty = useRef(false)
  const emptyValue = clamp(0, min, max)
  useEffect(() => {
    if (allowEmpty.current && value === emptyValue) return
    allowEmpty.current = false
    setDraft(String(value))
  }, [value, emptyValue])
  const sliderMin = Math.min(value, min === -maxDimension ? -128 : min)
  const sliderMax = Math.max(value, max === maxDimension ? 128 : max)
  return (
    <div className="field">
      <span>{label}</span>
      <div className="slider-row">
        <input
          type="range"
          aria-label={`${label} slider`}
          value={value}
          min={sliderMin}
          max={sliderMax}
          step={step}
          onChange={(event) => {
            const next = Number(event.target.value)
            allowEmpty.current = false
            setDraft(String(next))
            onChange(next)
          }}
        />
        <span className="number-wrap">
          <input
            type="number"
            aria-label={`${label} value`}
            value={draft}
            min={min}
            max={max}
            step={step}
            onChange={(event) => {
              const raw = event.target.value
              setDraft(raw)
              allowEmpty.current = raw === ""
              if (raw === "") onChange(emptyValue)
              else if (Number.isFinite(Number(raw)))
                onChange(clamp(Number(raw), min, max))
            }}
            onBlur={() => {
              if (draft !== "") setDraft(String(clamp(Number(draft), min, max)))
            }}
          />
          <em>{suffix}</em>
        </span>
      </div>
    </div>
  )
}

function ColorField({
  label,
  value,
  onChange,
}: {
  label: string
  value: string
  onChange: (value: string) => void
}) {
  return (
    <label className="field color-field">
      <span>{label}</span>
      <span className="color-wrap">
        <input
          type="color"
          value={/^#[0-9a-f]{6}$/i.test(value) ? value : "#ffffff"}
          onChange={(event) => onChange(event.target.value)}
        />
        <input
          aria-label={`${label} hex value`}
          value={value}
          maxLength={7}
          onChange={(event) => onChange(event.target.value)}
        />
      </span>
    </label>
  )
}

function Toggle({
  label,
  checked,
  onChange,
}: {
  label: string
  checked: boolean
  onChange: (checked: boolean) => void
}) {
  return (
    <label className="toggle">
      <input
        type="checkbox"
        checked={checked}
        onChange={(event) => onChange(event.target.checked)}
      />
      <span>{label}</span>
    </label>
  )
}

export default function App() {
  const [layers, setLayers] = useState<Layer[]>(initialLayers)
  const [selectedId, setSelectedId] = useState<string | null>(
    layers[0]?.id ?? null,
  )
  const [addType, setAddType] = useState<LayerType>("dot")
  const [previewMode, setPreviewMode] = useState(false)
  const [scene, setScene] = useState<(typeof scenes)[number]["id"]>("cs2")
  const [draggingId, setDraggingId] = useState<string | null>(null)
  const [dropTarget, setDropTarget] = useState<{
    id: string
    above: boolean
  } | null>(null)
  const [shareOpen, setShareOpen] = useState(false)
  const [importCode, setImportCode] = useState("")
  const [message, setMessage] = useState("")
  const [previewArea, setPreviewArea] = useState({ width: 500, height: 500 })
  const previewHost = useRef<HTMLDivElement>(null)
  const previewCanvas = useRef<HTMLCanvasElement>(null)
  const fullscreenHost = useRef<HTMLDivElement>(null)
  const fullscreenCanvas = useRef<HTMLCanvasElement>(null)
  const fileInput = useRef<HTMLInputElement>(null)

  const selected = layers.find((layer) => layer.id === selectedId)
  const sceneImage = scenes.find((item) => item.id === scene)!.image
  const size = useMemo(() => canvasSize(layers), [layers])
  const hasImage = layers.some((layer) => layer.type === "image")
  const tooLarge =
    size.width > 8192 ||
    size.height > 8192 ||
    size.width * size.height > 16_000_000

  function update(key: keyof Layer, value: unknown) {
    setLayers((current) =>
      current.map((layer) =>
        layer.id === selectedId ? { ...layer, [key]: value } : layer,
      ),
    )
  }
  function number(key: NumberKey, value: number) {
    update(key, value)
  }
  function reorder(dragId: string, targetId: string, above: boolean) {
    if (dragId === targetId) return
    setLayers((current) => {
      const next = [...current]
      const from = next.findIndex((layer) => layer.id === dragId)
      if (from < 0) return current
      const [moved] = next.splice(from, 1)
      const target = next.findIndex((layer) => layer.id === targetId)
      if (target < 0) return current
      next.splice(target + (above ? 1 : 0), 0, moved)
      return next
    })
  }
  function dragOver(event: DragEvent<HTMLDivElement>, id: string) {
    if (!draggingId || draggingId === id) return
    event.preventDefault()
    event.dataTransfer.dropEffect = "move"
    const above =
      event.clientY <
      event.currentTarget.getBoundingClientRect().top +
        event.currentTarget.clientHeight / 2
    setDropTarget({ id, above })
  }
  function deleteLayer(id: string) {
    setLayers((current) => current.filter((item) => item.id !== id))
    if (selectedId === id)
      setSelectedId(layers.find((item) => item.id !== id)?.id ?? null)
  }
  function add(type: LayerType) {
    if (type === "image") {
      fileInput.current?.click()
      return
    }
    const layer = newLayer(type)
    setLayers((current) => [...current, layer])
    setSelectedId(layer.id)
  }
  async function upload(file?: File) {
    if (!file) return
    if (!file.type.startsWith("image/")) {
      setMessage("Choose an image file.")
      return
    }
    const src = await new Promise<string>((resolve, reject) => {
      const reader = new FileReader()
      reader.onload = () => resolve(String(reader.result))
      reader.onerror = () => reject(new Error("Image read failed"))
      reader.readAsDataURL(file)
    })
    const image = new Image()
    image.onload = () => {
      const layer = newLayer("image")
      layer.name =
        file.name.replace(/\.[^.]+$/, "").slice(0, 40) || "Custom image"
      layer.imageName = file.name
      layer.imageSrc = src
      layer.width = Math.min(image.naturalWidth, maxDimension)
      layer.height = Math.max(
        1,
        Math.round((layer.width * image.naturalHeight) / image.naturalWidth),
      )
      layer.imageAspect = image.naturalWidth / image.naturalHeight
      layer.radius = 0
      setLayers((current) => [...current, layer])
      setSelectedId(layer.id)
      setMessage("")
    }
    image.onerror = () => setMessage("Could not read that image.")
    image.src = src
  }
  function imageDimension(key: "width" | "height", value: number) {
    if (!selected) return
    if (selected.lockAspect) {
      const other = key === "width" ? "height" : "width"
      const otherValue =
        key === "width"
          ? Math.round(value / selected.imageAspect)
          : Math.round(value * selected.imageAspect)
      setLayers((current) =>
        current.map((layer) =>
          layer.id === selectedId
            ? {
                ...layer,
                [key]: value,
                [other]: clamp(otherValue, 1, maxDimension),
              }
            : layer,
        ),
      )
    } else update(key, value)
  }

  useEffect(() => {
    const preventTab = (event: KeyboardEvent) => {
      if (
        event.key === "Tab" &&
        !event.ctrlKey &&
        !event.metaKey &&
        !event.altKey
      )
        event.preventDefault()
    }
    document.addEventListener("keydown", preventTab, true)
    return () => document.removeEventListener("keydown", preventTab, true)
  }, [])

  useEffect(() => {
    if (!previewHost.current) return
    const observer = new ResizeObserver((entries) => {
      const rect = entries[0].contentRect
      setPreviewArea({ width: rect.width, height: rect.height })
    })
    observer.observe(previewHost.current)
    return () => observer.disconnect()
  }, [])

  useEffect(() => {
    const canvas = previewCanvas.current
    if (!canvas) return
    const dpr = devicePixelRatio || 1
    const width = Math.max(1, Math.floor(previewArea.width * dpr))
    const height = Math.max(1, Math.floor(previewArea.height * dpr))
    canvas.width = width
    canvas.height = height
    const scale = Math.min(
      (previewArea.width - 70) / size.width,
      (previewArea.height - 70) / size.height,
      3,
    )
    render(
      canvas.getContext("2d")!,
      layers,
      width,
      height,
      Math.max(0.05, scale) * dpr,
      () => {
        const ctx = canvas.getContext("2d")
        if (ctx) render(ctx, layers, width, height, Math.max(0.05, scale) * dpr)
      },
    )
  }, [layers, previewArea, size])

  useEffect(() => {
    if (!previewMode || !fullscreenCanvas.current) return
    const canvas = fullscreenCanvas.current
    const dpr = devicePixelRatio || 1
    canvas.width = Math.max(1, Math.ceil(size.width * dpr))
    canvas.height = Math.max(1, Math.ceil(size.height * dpr))
    render(
      canvas.getContext("2d")!,
      layers,
      canvas.width,
      canvas.height,
      dpr,
      () =>
        render(
          canvas.getContext("2d")!,
          layers,
          canvas.width,
          canvas.height,
          dpr,
        ),
    )
  }, [layers, previewMode, size])

  useEffect(() => {
    const changed = () => {
      if (!document.fullscreenElement) setPreviewMode(false)
    }
    document.addEventListener("fullscreenchange", changed)
    return () => document.removeEventListener("fullscreenchange", changed)
  }, [])

  async function openFullscreen() {
    flushSync(() => setPreviewMode(true))
    try {
      await fullscreenHost.current?.requestFullscreen()
    } catch {
      setMessage(
        "Browser fullscreen was unavailable. Showing the preview in-page.",
      )
    }
  }
  async function closeFullscreen() {
    if (document.fullscreenElement) await document.exitFullscreen()
    setPreviewMode(false)
  }
  async function download() {
    if (tooLarge) {
      setMessage(
        "The design is too large to export. Reduce its size or offsets.",
      )
      return
    }
    try {
      await preloadImages(layers)
      const canvas = document.createElement("canvas")
      canvas.width = size.width
      canvas.height = size.height
      render(canvas.getContext("2d")!, layers, canvas.width, canvas.height)
      const blob = await new Promise<Blob>((resolve, reject) =>
        canvas.toBlob(
          (value) =>
            value ? resolve(value) : reject(new Error("PNG export failed")),
          "image/png",
        ),
      )
      const url = URL.createObjectURL(blob)
      const link = document.createElement("a")
      link.href = url
      link.download = "crosshair.png"
      link.click()
      setTimeout(() => URL.revokeObjectURL(url), 1000)
      setMessage("PNG downloaded.")
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "PNG export failed.")
    }
  }
  function importShare() {
    try {
      const decoded = decodeShare(importCode)
      setLayers(decoded)
      setSelectedId(decoded[0]?.id ?? null)
      setShareOpen(false)
      setMessage("Share code loaded.")
    } catch {
      setMessage("Invalid share code.")
    }
  }

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">
          <span className="brand-mark">+</span>
          <strong>wlcrosshair</strong>
          <span className="brand-suffix">generator</span>
        </div>
        <div className="toolbar">
          <button onClick={openFullscreen}>Fullscreen preview</button>
          <button onClick={() => setShareOpen(true)}>Share code</button>
          <button className="primary" onClick={download} disabled={tooLarge}>
            Download PNG
          </button>
        </div>
      </header>
      <main className="workspace">
        <aside className="sidebar layers-panel">
          <div className="panel-heading">
            <h2>Layers</h2>
            <span>{layers.length}</span>
          </div>
          <div className="layer-list">
            {[...layers].reverse().map((layer) => (
              <div
                key={layer.id}
                className={`layer-row ${selectedId === layer.id ? "selected" : ""} ${draggingId === layer.id ? "dragging" : ""} ${dropTarget?.id === layer.id ? (dropTarget.above ? "drop-above" : "drop-below") : ""}`}
                draggable
                onDragStart={(event) => {
                  event.dataTransfer.effectAllowed = "move"
                  event.dataTransfer.setData("text/plain", layer.id)
                  setDraggingId(layer.id)
                }}
                onDragOver={(event) => dragOver(event, layer.id)}
                onDrop={(event) => {
                  event.preventDefault()
                  const dragId = event.dataTransfer.getData("text/plain")
                  const above =
                    event.clientY <
                    event.currentTarget.getBoundingClientRect().top +
                      event.currentTarget.clientHeight / 2
                  if (dragId) reorder(dragId, layer.id, above)
                  setDraggingId(null)
                  setDropTarget(null)
                }}
                onDragEnd={() => {
                  setDraggingId(null)
                  setDropTarget(null)
                }}
              >
                <button
                  className="layer-main"
                  onClick={() => setSelectedId(layer.id)}
                >
                  <span className="layer-icon">
                    {layer.type === "cross"
                      ? "+"
                      : layer.type === "dot"
                        ? "•"
                        : layer.type === "ring"
                          ? "○"
                          : layer.type === "chevron"
                            ? "⌃"
                            : "▧"}
                  </span>
                  <span className="layer-text">
                    <strong>{layer.name}</strong>
                    <small>{typeNames[layer.type]}</small>
                  </span>
                  <span className="drag-handle" aria-hidden="true">
                    ⠿
                  </span>
                </button>
                <div className="layer-actions">
                  <button
                    title="Toggle visibility"
                    aria-label={`Toggle ${layer.name} visibility`}
                    onClick={() =>
                      setLayers((current) =>
                        current.map((item) =>
                          item.id === layer.id
                            ? { ...item, visible: !item.visible }
                            : item,
                        ),
                      )
                    }
                  >
                    {layer.visible ? "◉" : "○"}
                  </button>
                  <button
                    className="delete-layer"
                    title="Delete layer"
                    aria-label={`Delete ${layer.name}`}
                    onClick={() => deleteLayer(layer.id)}
                  >
                    ×
                  </button>
                </div>
              </div>
            ))}
          </div>
          <div className="add-layer">
            <select
              aria-label="Layer type"
              value={addType}
              onChange={(event) => setAddType(event.target.value as LayerType)}
            >
              {kinds.map((kind) => (
                <option key={kind} value={kind}>
                  {typeNames[kind]}
                </option>
              ))}
            </select>
            <button className="primary" onClick={() => add(addType)}>
              Add layer
            </button>
            <input
              ref={fileInput}
              hidden
              type="file"
              accept="image/*"
              onChange={(event) => {
                void upload(event.target.files?.[0])
                event.target.value = ""
              }}
            />
          </div>
          <p className="panel-note">
            Drag cards to change their order. Top layers draw over lower layers.
            Image layers disable share codes.
          </p>
        </aside>
        <section className="preview-panel">
          <div className="preview-heading">
            <h2>Preview</h2>
            <span>Scaled to fit</span>
          </div>
          <div className="preview-stage" ref={previewHost}>
            <div className="axis axis-x" />
            <div className="axis axis-y" />
            <canvas ref={previewCanvas} />
          </div>
          <div className="preview-footer">
            <span>
              PNG size:{" "}
              <strong>
                {size.width} × {size.height} px
              </strong>
            </span>
            <span>
              {tooLarge ? "Too large to export" : "Transparent background"}
            </span>
          </div>
        </section>
        <aside className="sidebar settings-panel">
          <div className="panel-heading">
            <h2>Settings</h2>
          </div>
          {selected ? (
            <div className="settings-scroll">
              <label className="field">
                <span>Name</span>
                <input
                  value={selected.name}
                  maxLength={40}
                  onChange={(event) => update("name", event.target.value)}
                />
              </label>
              <div className="section-title">Placement</div>
              <div className="field-grid">
                <NumberField
                  label="X offset"
                  value={selected.x}
                  min={-2048}
                  onChange={(value) => number("x", value)}
                />
                <NumberField
                  label="Y offset"
                  value={selected.y}
                  min={-2048}
                  onChange={(value) => number("y", value)}
                />
                <NumberField
                  label="Rotation"
                  value={selected.rotation}
                  min={-360}
                  max={360}
                  suffix="°"
                  onChange={(value) => number("rotation", value)}
                />
                <NumberField
                  label="Layer opacity"
                  value={selected.opacity}
                  max={100}
                  suffix="%"
                  onChange={(value) => number("opacity", value)}
                />
              </div>
              <div className="section-title">Shape</div>
              {selected.type === "dot" && (
                <div className="field-grid">
                  <NumberField
                    label="Width"
                    value={selected.width}
                    min={1}
                    onChange={(value) => number("width", value)}
                  />
                  <NumberField
                    label="Height"
                    value={selected.height}
                    min={1}
                    onChange={(value) => number("height", value)}
                  />
                  <NumberField
                    label="Corner radius"
                    value={selected.radius}
                    onChange={(value) => number("radius", value)}
                  />
                </div>
              )}
              {selected.type === "cross" && (
                <>
                  <div className="field-grid">
                    <NumberField
                      label="Horizontal length"
                      value={selected.horizontalLength}
                      onChange={(value) => number("horizontalLength", value)}
                    />
                    <NumberField
                      label="Horizontal width"
                      value={selected.horizontalThickness}
                      min={1}
                      onChange={(value) => number("horizontalThickness", value)}
                    />
                    <NumberField
                      label="Horizontal gap"
                      value={selected.horizontalGap}
                      onChange={(value) => number("horizontalGap", value)}
                    />
                    <NumberField
                      label="Vertical length"
                      value={selected.verticalLength}
                      onChange={(value) => number("verticalLength", value)}
                    />
                    <NumberField
                      label="Vertical width"
                      value={selected.verticalThickness}
                      min={1}
                      onChange={(value) => number("verticalThickness", value)}
                    />
                    <NumberField
                      label="Vertical gap"
                      value={selected.verticalGap}
                      onChange={(value) => number("verticalGap", value)}
                    />
                  </div>
                  <div className="arm-grid">
                    {(["Top", "Right", "Bottom", "Left"] as const).map(
                      (name, index) => (
                        <Toggle
                          key={name}
                          label={name}
                          checked={selected.arms[index]}
                          onChange={(checked) => {
                            const arms = [...selected.arms] as Layer["arms"]
                            arms[index] = checked
                            update("arms", arms)
                          }}
                        />
                      ),
                    )}
                  </div>
                </>
              )}
              {selected.type === "ring" && (
                <div className="field-grid">
                  <NumberField
                    label="Outer width"
                    value={selected.width}
                    min={1}
                    onChange={(value) => number("width", value)}
                  />
                  <NumberField
                    label="Outer height"
                    value={selected.height}
                    min={1}
                    onChange={(value) => number("height", value)}
                  />
                  <NumberField
                    label="Stroke width"
                    value={selected.strokeWidth}
                    min={1}
                    onChange={(value) => number("strokeWidth", value)}
                  />
                </div>
              )}
              {selected.type === "chevron" && (
                <div className="field-grid">
                  <NumberField
                    label="Arm length"
                    value={selected.horizontalLength}
                    min={1}
                    onChange={(value) => number("horizontalLength", value)}
                  />
                  <NumberField
                    label="Stroke width"
                    value={selected.strokeWidth}
                    min={1}
                    onChange={(value) => number("strokeWidth", value)}
                  />
                  <NumberField
                    label="Angle"
                    value={selected.angle}
                    min={1}
                    max={179}
                    suffix="°"
                    onChange={(value) => number("angle", value)}
                  />
                  <NumberField
                    label="Tip gap"
                    value={selected.tipGap}
                    onChange={(value) => number("tipGap", value)}
                  />
                  <label className="field">
                    <span>Direction</span>
                    <select
                      value={selected.direction}
                      onChange={(event) =>
                        number("direction", Number(event.target.value))
                      }
                    >
                      <option value={0}>Up</option>
                      <option value={1}>Right</option>
                      <option value={2}>Down</option>
                      <option value={3}>Left</option>
                    </select>
                  </label>
                </div>
              )}
              {selected.type === "image" && (
                <>
                  <div className="field-grid">
                    <NumberField
                      label="Width"
                      value={selected.width}
                      min={1}
                      onChange={(value) => imageDimension("width", value)}
                    />
                    <NumberField
                      label="Height"
                      value={selected.height}
                      min={1}
                      onChange={(value) => imageDimension("height", value)}
                    />
                  </div>
                  <Toggle
                    label="Lock aspect ratio"
                    checked={selected.lockAspect}
                    onChange={(value) => update("lockAspect", value)}
                  />
                  <Toggle
                    label="Smooth scaling"
                    checked={selected.smoothing}
                    onChange={(value) => update("smoothing", value)}
                  />
                  <p className="panel-note">
                    Border follows visible image pixels.
                  </p>
                </>
              )}
              <div className="section-title">
                {selected.type === "ring" || selected.type === "chevron"
                  ? "Stroke"
                  : selected.type === "image"
                    ? "Image"
                    : "Fill"}
              </div>
              {selected.type !== "image" && (
                <ColorField
                  label="Color"
                  value={selected.color}
                  onChange={(value) => update("color", value)}
                />
              )}
              <NumberField
                label="Opacity"
                value={selected.paintOpacity}
                max={100}
                suffix="%"
                onChange={(value) => number("paintOpacity", value)}
              />
              <div className="section-title">Border</div>
              <ColorField
                label="Color"
                value={selected.borderColor}
                onChange={(value) => update("borderColor", value)}
              />
              <div className="field-grid">
                <NumberField
                  label="Width"
                  value={selected.borderWidth}
                  max={64}
                  onChange={(value) => number("borderWidth", value)}
                />
                <NumberField
                  label="Opacity"
                  value={selected.borderOpacity}
                  max={100}
                  suffix="%"
                  onChange={(value) => number("borderOpacity", value)}
                />
              </div>
            </div>
          ) : (
            <p className="empty-settings">Select a layer to edit it.</p>
          )}
        </aside>
      </main>
      {message && (
        <div className="toast" role="status">
          {message}
          <button aria-label="Dismiss" onClick={() => setMessage("")}>
            ×
          </button>
        </div>
      )}
      {previewMode && (
        <div
          className="fullscreen"
          ref={fullscreenHost}
          style={{ backgroundImage: `url(${sceneImage})` }}
        >
          <div className="fullscreen-controls">
            <label className="scene-control">
              <span>Background</span>
              <select
                value={scene}
                onChange={(event) =>
                  setScene(event.target.value as typeof scene)
                }
              >
                {scenes.map((item) => (
                  <option key={item.id} value={item.id}>
                    {item.label}
                  </option>
                ))}
              </select>
            </label>
            <button onClick={() => void closeFullscreen()}>Exit preview</button>
          </div>
          <canvas
            ref={fullscreenCanvas}
            style={{ width: size.width, height: size.height }}
          />
          <span className="fullscreen-size">
            {size.width} × {size.height} CSS px · 100%
          </span>
        </div>
      )}
      {shareOpen && (
        <div
          className="modal-backdrop"
          onMouseDown={(event) => {
            if (event.target === event.currentTarget) setShareOpen(false)
          }}
        >
          <div
            className="modal"
            role="dialog"
            aria-modal="true"
            aria-label="Share code"
          >
            <div className="modal-heading">
              <h2>Share code</h2>
              <button onClick={() => setShareOpen(false)} aria-label="Close">
                ×
              </button>
            </div>
            {!hasImage && (
              <>
                <p>Copy this code to share the layers and settings.</p>
                <textarea readOnly value={encodeShare(layers)} rows={4} />
                <button
                  className="primary"
                  onClick={() =>
                    void navigator.clipboard
                      .writeText(encodeShare(layers))
                      .then(() => setMessage("Share code copied."))
                      .catch(() =>
                        setMessage("Copy failed. Select the code above."),
                      )
                  }
                >
                  Copy code
                </button>
              </>
            )}
            {hasImage && (
              <p>
                Share codes are unavailable while the design has a custom image
                layer.
              </p>
            )}
            <div className="section-title">Open a share code</div>
            <textarea
              value={importCode}
              onChange={(event) => setImportCode(event.target.value)}
              placeholder="Paste a share code"
              rows={3}
            />
            <button onClick={importShare}>Load code</button>
          </div>
        </div>
      )}
    </div>
  )
}
