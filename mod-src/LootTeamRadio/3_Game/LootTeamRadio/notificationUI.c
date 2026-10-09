// Keep only Loot.Team radio notifications at the top of the vanilla stack.
// Remove their widgets immediately on expiration instead of using vanilla's
// shared fade timer map, whose string keys may collide under rapid tuning.
modded class NotificationUI
{
	private int m_LootRadioNotificationSort;

	static bool IsLootRadioNotification(NotificationRuntimeData data)
	{
		if (!data) return false;
		string title = data.GetTitleText();
		return title.IndexOf("FM: ") == 0 || title.IndexOf("Смена FM: ") == 0 || title.IndexOf("Radio: ") == 0 || title.IndexOf("Радио: ") == 0;
	}

	override void AddNotification(NotificationRuntimeData data)
	{
		super.AddNotification(data);
		if (!IsLootRadioNotification(data) || !m_Notifications.Contains(data)) return;
		Widget widget = m_Notifications.Get(data);
		if (!widget) return;
		// GridSpacer sorts children by sort value: newer radio messages go first.
		m_LootRadioNotificationSort++;
		widget.SetSort(-1000 - m_LootRadioNotificationSort);
		UpdateTargetHeight();
	}

	override void RemoveNotification(NotificationRuntimeData data)
	{
		if (!IsLootRadioNotification(data) || !m_Notifications.Contains(data))
		{
			super.RemoveNotification(data);
			return;
		}
		Widget widget = m_Notifications.Get(data);
		m_Notifications.Remove(data);
		if (widget) delete widget;
		UpdateTargetHeight();
	}
}
