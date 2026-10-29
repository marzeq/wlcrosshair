export type LayerType = "dot" | "cross" | "ring" | "chevron" | "image"

export interface Layer {
  id: string
  type: LayerType
  name: string
  visible: boolean
  x: number
  y: number
  rotation: number
  opacity: number
  color: string
  paintOpacity: number
  borderColor: string
  borderOpacity: number
  borderWidth: number
  width: number
  height: number
  radius: number
  horizontalLength: number
  verticalLength: number
  horizontalThickness: number
  verticalThickness: number
  horizontalGap: number
  verticalGap: number
  arms: [boolean, boolean, boolean, boolean]
  strokeWidth: number
  angle: number
  direction: number
  tipGap: number
  imageSrc?: string
  imageName?: string
  lockAspect: boolean
  imageAspect: number
  smoothing: boolean
}

export const typeNames: Record<LayerType, string> = {
  dot: "Dot",
  cross: "Cross",
  ring: "Ring",
  chevron: "Chevron",
  image: "Custom image",
}

export function newLayer(type: LayerType): Layer {
  return {
    id: crypto.randomUUID(),
    type,
    name: typeNames[type],
    visible: true,
    x: 0,
    y: 0,
    rotation: 0,
    opacity: 100,
    color: "#39a4ff",
    paintOpacity: 100,
    borderColor: "#10151d",
    borderOpacity: 100,
    borderWidth: 0,
    width: type === "ring" ? 32 : 6,
    height: type === "ring" ? 32 : 6,
    radius: 3,
    horizontalLength: 12,
    verticalLength: 12,
    horizontalThickness: 3,
    verticalThickness: 3,
    horizontalGap: 4,
    verticalGap: 4,
    arms: [true, true, true, true],
    strokeWidth: 3,
    angle: 90,
    direction: 0,
    tipGap: 4,
    lockAspect: true,
    imageAspect: 1,
    smoothing: true,
  }
}

export const initialLayers: Layer[] = [
  {
    ...newLayer("cross"),
    id: "initial-cross",
    name: "Cross",
    color: "#4aa8ff",
  },
  {
    ...newLayer("dot"),
    id: "initial-dot",
    name: "Center dot",
    width: 3,
    height: 3,
    radius: 2,
    color: "#ffffff",
  },
]

export function clamp(value: number, min: number, max: number): number {
  return Number.isFinite(value) ? Math.min(max, Math.max(min, value)) : min
}

const shareKeys: (keyof Layer)[] = [
  "type",
  "name",
  "visible",
  "x",
  "y",
  "rotation",
  "opacity",
  "color",
  "paintOpacity",
  "borderColor",
  "borderOpacity",
  "borderWidth",
  "width",
  "height",
  "radius",
  "horizontalLength",
  "verticalLength",
  "horizontalThickness",
  "verticalThickness",
  "horizontalGap",
  "verticalGap",
  "arms",
  "strokeWidth",
  "angle",
  "direction",
  "tipGap",
  "lockAspect",
  "imageAspect",
  "smoothing",
]
const shareFields = shareKeys.slice(1)
const shareTypes: LayerType[] = ["dot", "cross", "ring", "chevron"]

function assignSharedValue(layer: Layer, key: keyof Layer, value: unknown) {
  if (key === "arms") {
    if (
      Array.isArray(value) &&
      value.length === 4 &&
      value.every((item) => typeof item === "boolean")
    )
      layer.arms = value as Layer["arms"]
    return
  }
  if (typeof layer[key] === "number" && typeof value === "number")
    (layer as unknown as Record<string, unknown>)[key] = clamp(
      value,
      -10000,
      10000,
    )
  else if (typeof layer[key] === "boolean" && typeof value === "boolean")
    (layer as unknown as Record<string, unknown>)[key] = value
  else if (typeof layer[key] === "string" && typeof value === "string")
    (layer as unknown as Record<string, unknown>)[key] = value.slice(0, 100)
}

export function encodeShare(layers: Layer[]): string {
  if (layers.some((layer) => layer.type === "image"))
    throw new Error("Custom images cannot be shared")
  const payload = [
    2,
    layers.map((layer) => {
      const defaults = newLayer(layer.type)
      const values = shareFields.map((key) =>
        JSON.stringify(layer[key]) === JSON.stringify(defaults[key])
          ? null
          : layer[key],
      )
      while (values.at(-1) === null) values.pop()
      return [shareTypes.indexOf(layer.type), values]
    }),
  ]
  const bytes = new TextEncoder().encode(JSON.stringify(payload))
  let binary = ""
  bytes.forEach((byte) => {
    binary += String.fromCharCode(byte)
  })
  return (
    "WLC2-" +
    btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "")
  )
}

export function decodeShare(code: string): Layer[] {
  const clean = code.trim().replace(/\s+/g, "")
  if (clean.length > 100000) throw new Error("Invalid share code")
  const encoded = clean.startsWith("WLC2-") ? clean.slice(5) : clean
  const binary = atob(encoded.replace(/-/g, "+").replace(/_/g, "/"))
  const bytes = Uint8Array.from(binary, (character) => character.charCodeAt(0))
  const parsed: unknown = JSON.parse(new TextDecoder().decode(bytes))
  if (
    Array.isArray(parsed) &&
    parsed[0] === 2 &&
    Array.isArray(parsed[1]) &&
    parsed[1].length <= 100
  ) {
    return parsed[1].map((entry: unknown) => {
      if (
        !Array.isArray(entry) ||
        !Number.isInteger(entry[0]) ||
        entry[0] < 0 ||
        entry[0] >= shareTypes.length ||
        !Array.isArray(entry[1]) ||
        entry[1].length > shareFields.length
      )
        throw new Error("Invalid layer")
      const layer = newLayer(shareTypes[entry[0]])
      entry[1].forEach((value: unknown, index: number) => {
        if (value !== null) assignSharedValue(layer, shareFields[index], value)
      })
      return layer
    })
  }
  if (
    !parsed ||
    typeof parsed !== "object" ||
    !("version" in parsed) ||
    parsed.version !== 1 ||
    !("layers" in parsed) ||
    !Array.isArray(parsed.layers) ||
    parsed.layers.length > 100
  ) {
    throw new Error("Invalid share code")
  }
  return parsed.layers.map((raw: unknown) => {
    if (
      !raw ||
      typeof raw !== "object" ||
      !("type" in raw) ||
      typeof raw.type !== "string" ||
      !(raw.type in typeNames) ||
      raw.type === "image"
    )
      throw new Error("Invalid layer")
    const base = newLayer(raw.type as LayerType)
    const input = raw as Record<string, unknown>
    for (const key of shareFields) assignSharedValue(base, key, input[key])
    return base
  })
}
