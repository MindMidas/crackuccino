interface MessageBannerProps {
  type: "error" | "success";
  title: string;
  message: string;
  className?: string;
}

export function MessageBanner({ type, title, message, className = "" }: MessageBannerProps) {
  return (
    <div
      className={`message-banner message-banner-${type} ${className}`.trim()}
      role={type === "error" ? "alert" : "status"}
    >
      <p className="message-banner-title">{title}</p>
      <p className="message-banner-body">{message}</p>
    </div>
  );
}
