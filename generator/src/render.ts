import type { Layer } from "./model"

const imageCache = new Map<string, HTMLImageElement>()

function imageFor(src: string, changed?: () => void): HTMLImageElement {
  let image = imageCache.get(src)
  if (!image) {
    image = new Image()
    image.src = src
    imageCache.set(src, image)
  }
  if (!image.complete && changed)
    image.addEventListener("load", changed, { once: true })
  return image
}

export async function preloadImages(layers: Layer[]): Promise<void> {
  await Promise.all(
    layers
      .filter((layer) => layer.type === "image" && layer.imageSrc)
      .map((layer) => {
        const image = imageFor(layer.imageSrc!)
        if (image.complete && image.naturalWidth) return Promise.resolve()
        return new Promise<void>((resolve, reject) => {
          image.addEventListener("load", () => resolve(), { once: true })
          image.addEventListener(
            "error",
            () =>
              reject(new Error(`Could not load ${layer.imageName || "image"}`)),
            { once: true },
          )
        })
      }),
  )
}

function crossRects(layer: Layer): [number, number, number, number][] {
  const [top, right, bottom, left] = layer.arms
  const h = Math.max(0, layer.horizontalThickness)
  const v = Math.max(0, layer.verticalThickness)
  const result: [number, number, number, number][] = []
  if (right)
    result.push([layer.horizontalGap, -h / 2, layer.horizontalLength, h])
  if (left)
    result.push([
      -layer.horizontalGap - layer.horizontalLength,
      -h / 2,
      layer.horizontalLength,
      h,
    ])
  if (top)
    result.push([
      -v / 2,
      -layer.verticalGap - layer.verticalLength,
      v,
      layer.verticalLength,
    ])
  if (bottom) result.push([-v / 2, layer.verticalGap, v, layer.verticalLength])
  return result
}

function pathFor(layer: Layer): Path2D {
  const path = new Path2D()
  if (layer.type === "dot") {
    path.roundRect(
      -layer.width / 2,
      -layer.height / 2,
      layer.width,
      layer.height,
      Math.min(layer.radius, layer.width / 2, layer.height / 2),
    )
  } else if (layer.type === "cross") {
    crossRects(layer).forEach(([x, y, width, height]) =>
      path.rect(x, y, width, height),
    )
  } else if (layer.type === "ring") {
    path.ellipse(
      0,
      0,
      Math.max(0.1, (layer.width - layer.strokeWidth) / 2),
      Math.max(0.1, (layer.height - layer.strokeWidth) / 2),
      0,
      0,
      Math.PI * 2,
    )
  } else if (layer.type === "chevron") {
    const half = (layer.angle * Math.PI) / 360
    const spread = layer.horizontalLength * Math.sin(half)
    const reach = layer.horizontalLength * Math.cos(half)
    path.moveTo(-spread, -layer.tipGap - reach)
    path.lineTo(0, -layer.tipGap)
    path.lineTo(spread, -layer.tipGap - reach)
  }
  return path
}

function localBounds(layer: Layer): [number, number] {
  const border = layer.borderOpacity > 0 ? layer.borderWidth : 0
  switch (layer.type) {
    case "dot":
    case "image":
      return [layer.width / 2 + border, layer.height / 2 + border]
    case "ring":
      return [
        Math.max(layer.width / 2, layer.strokeWidth / 2 + 0.1) + border,
        Math.max(layer.height / 2, layer.strokeWidth / 2 + 0.1) + border,
      ]
    case "cross": {
      const [top, right, bottom, left] = layer.arms
      if (!layer.arms.some(Boolean)) return [0, 0]
      return [
        Math.max(
          right || left ? layer.horizontalGap + layer.horizontalLength : 0,
          top || bottom ? layer.verticalThickness / 2 : 0,
        ) + border,
        Math.max(
          top || bottom ? layer.verticalGap + layer.verticalLength : 0,
          right || left ? layer.horizontalThickness / 2 : 0,
        ) + border,
      ]
    }
    case "chevron": {
      const half = (layer.angle * Math.PI) / 360
      const spread = Math.abs(layer.horizontalLength * Math.sin(half))
      const reach = Math.abs(layer.horizontalLength * Math.cos(half))
      const pad = layer.strokeWidth / 2 + border
      return [spread + pad, layer.tipGap + reach + pad]
    }
  }
}

export function canvasSize(layers: Layer[]): { width: number; height: number } {
  let halfWidth = 0
  let halfHeight = 0
  for (const layer of layers) {
    if (
      !layer.visible ||
      layer.opacity <= 0 ||
      (layer.paintOpacity <= 0 &&
        (layer.borderOpacity <= 0 || layer.borderWidth <= 0))
    )
      continue
    const [localX, localY] = localBounds(layer)
    const rotation =
      ((layer.rotation +
        (layer.type === "chevron" ? layer.direction * 90 : 0)) *
        Math.PI) /
      180
    const dx =
      Math.abs(Math.cos(rotation)) * localX +
      Math.abs(Math.sin(rotation)) * localY
    const dy =
      Math.abs(Math.sin(rotation)) * localX +
      Math.abs(Math.cos(rotation)) * localY
    halfWidth = Math.max(halfWidth, Math.abs(layer.x) + dx)
    halfHeight = Math.max(halfHeight, Math.abs(layer.y) + dy)
  }
  return {
    width: Math.max(1, Math.ceil(halfWidth * 2)),
    height: Math.max(1, Math.ceil(halfHeight * 2)),
  }
}

function layerTransform(
  ctx: CanvasRenderingContext2D,
  layer: Layer,
  width: number,
  height: number,
  scale: number,
  draw: () => void,
) {
  ctx.save()
  ctx.translate(width / 2, height / 2)
  ctx.scale(scale, scale)
  ctx.translate(layer.x, layer.y)
  ctx.rotate(
    ((layer.rotation + (layer.type === "chevron" ? layer.direction * 90 : 0)) *
      Math.PI) /
      180,
  )
  ctx.lineJoin = "round"
  ctx.lineCap = "round"
  draw()
  ctx.restore()
}

function drawPaintShape(
  ctx: CanvasRenderingContext2D,
  layer: Layer,
  mask: boolean,
  changed?: () => void,
) {
  ctx.globalAlpha = mask
    ? 1
    : ((layer.opacity / 100) * layer.paintOpacity) / 100
  if (layer.type === "image") {
    if (!layer.imageSrc) return
    const image = imageFor(layer.imageSrc, changed)
    if (!image.complete || !image.naturalWidth) return
    ctx.imageSmoothingEnabled = layer.smoothing
    ctx.drawImage(
      image,
      -layer.width / 2,
      -layer.height / 2,
      layer.width,
      layer.height,
    )
    return
  }
  const path = pathFor(layer)
  ctx.fillStyle = mask ? "#fff" : layer.color
  ctx.strokeStyle = mask ? "#fff" : layer.color
  if (layer.type === "ring" || layer.type === "chevron") {
    ctx.lineWidth = layer.strokeWidth
    ctx.stroke(path)
  } else ctx.fill(path)
}

function drawOuterBorder(
  ctx: CanvasRenderingContext2D,
  layer: Layer,
  changed?: () => void,
) {
  const b = layer.borderWidth
  ctx.globalAlpha = 1
  ctx.fillStyle = layer.borderColor
  ctx.strokeStyle = layer.borderColor
  if (layer.type === "dot") {
    const path = new Path2D()
    path.roundRect(
      -layer.width / 2 - b,
      -layer.height / 2 - b,
      layer.width + b * 2,
      layer.height + b * 2,
      Math.min(layer.radius + b, layer.width / 2 + b, layer.height / 2 + b),
    )
    ctx.fill(path)
  } else if (layer.type === "cross") {
    const path = new Path2D()
    crossRects(layer).forEach(([x, y, width, height]) =>
      path.rect(x - b, y - b, width + b * 2, height + b * 2),
    )
    ctx.fill(path)
  } else if (layer.type === "ring" || layer.type === "chevron") {
    ctx.lineWidth = layer.strokeWidth + b * 2
    ctx.stroke(pathFor(layer))
  } else if (layer.imageSrc) {
    const image = imageFor(layer.imageSrc, changed)
    if (!image.complete || !image.naturalWidth) return
    const tint = document.createElement("canvas")
    tint.width = Math.max(1, Math.ceil(layer.width))
    tint.height = Math.max(1, Math.ceil(layer.height))
    const tintCtx = tint.getContext("2d")!
    tintCtx.drawImage(image, 0, 0, tint.width, tint.height)
    tintCtx.globalCompositeOperation = "source-in"
    tintCtx.fillStyle = layer.borderColor
    tintCtx.fillRect(0, 0, tint.width, tint.height)
    const radius = Math.ceil(b)
    for (let y = -radius; y <= radius; y++) {
      for (let x = -radius; x <= radius; x++) {
        if (x * x + y * y > b * b) continue
        ctx.drawImage(
          tint,
          -layer.width / 2 + x,
          -layer.height / 2 + y,
          layer.width,
          layer.height,
        )
      }
    }
  }
}

export function render(
  ctx: CanvasRenderingContext2D,
  layers: Layer[],
  width: number,
  height: number,
  scale = 1,
  changed?: () => void,
) {
  ctx.clearRect(0, 0, width, height)
  const borders = layers.filter(
    (layer) =>
      layer.visible &&
      layer.opacity > 0 &&
      layer.borderWidth > 0 &&
      layer.borderOpacity > 0,
  )
  if (borders.length) {
    const borderCanvas = document.createElement("canvas")
    const paintMask = document.createElement("canvas")
    const singleBorder = document.createElement("canvas")
    for (const canvas of [borderCanvas, paintMask, singleBorder]) {
      canvas.width = width
      canvas.height = height
    }
    const borderCtx = borderCanvas.getContext("2d")!
    const maskCtx = paintMask.getContext("2d")!
    const singleCtx = singleBorder.getContext("2d")!
    for (const layer of borders) {
      singleCtx.clearRect(0, 0, width, height)
      layerTransform(singleCtx, layer, width, height, scale, () =>
        drawOuterBorder(singleCtx, layer, changed),
      )
      singleCtx.globalCompositeOperation = "destination-out"
      layerTransform(singleCtx, layer, width, height, scale, () =>
        drawPaintShape(singleCtx, layer, true, changed),
      )
      singleCtx.globalCompositeOperation = "source-over"
      borderCtx.globalAlpha =
        ((layer.opacity / 100) * layer.borderOpacity) / 100
      borderCtx.drawImage(singleBorder, 0, 0)
    }
    borderCtx.globalAlpha = 1
    for (const layer of layers) {
      if (!layer.visible || layer.opacity <= 0 || layer.paintOpacity <= 0)
        continue
      layerTransform(maskCtx, layer, width, height, scale, () =>
        drawPaintShape(maskCtx, layer, true, changed),
      )
    }
    borderCtx.globalCompositeOperation = "destination-out"
    borderCtx.drawImage(paintMask, 0, 0)
    ctx.drawImage(borderCanvas, 0, 0)
  }
  for (const layer of layers) {
    if (!layer.visible || layer.opacity <= 0 || layer.paintOpacity <= 0)
      continue
    layerTransform(ctx, layer, width, height, scale, () =>
      drawPaintShape(ctx, layer, false, changed),
    )
  }
}
